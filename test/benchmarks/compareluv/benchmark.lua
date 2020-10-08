local uvtime = [[
	(function ()
		package.cpath = os.getenv("LUV_CPATH").."/?.so;;"

		return require("luv").hrtime
	end)()
]]
local coutiltime = [[
	(function ()
		package.cpath = os.getenv("COUTIL_CPATH").."/?.so;"..
		                os.getenv("LUAMEM_CPATH").."/?.so;;"
		package.path = os.getenv("COUTIL_LPATH").."/?.lua;;"

		return require("coutil.system").nanosecs
	end)()
]]

local tcpcfg = [[
	local msgcount<const> = 65536
	local msgsize<const> = 512
	local msgbyte<const> = "x"
	local msgdata<const> = string.rep(msgbyte, msgsize)
	local host<const> = "127.0.0.1"
	local port<const> = 65432
	local backlog<const> = 8
]]
local idlecfg = [[
	local repeats<const> = 1e6
]]

return {
	id = "tcp",
	cases = {
		coutil = {
			lua = os.getenv("LUA54_BIN"),
			gettime = coutiltime,
			setup = [[
				local memory = require "memory"
				local system = require "coutil.system"
				local spawn = require "coutil.spawn"

				]]..tcpcfg..[[
				local address<const> = system.address("ipv4", host, port)

				spawn.catch(print, function ()
					local passive<close> = system.socket("passive", address.type)
					passive:bind(address)
					passive:listen(backlog)
					local stream<close> = passive:accept()
					local buffer<const> = memory.create(2*msgsize)
					local missing = msgcount*msgsize
					repeat
						missing = missing-stream:receive(buffer)
					until missing <= 0
				end)

				spawn.catch(print, function ()
					local stream<close> = system.socket("stream", address.type)
					stream:connect(address)
					for i = 1, msgcount do
						stream:send(msgdata)
					end
				end)
			]],
			test = [[system.run()]],
		},
		luv = {
			lua = os.getenv("LUA53_BIN"),
			gettime = uvtime,
			setup = [[
				local uv = require "luv"

				]]..tcpcfg:gsub("<const>", "")..[[
				local msgpattern = "^"..msgbyte.."+$"

				local passive = uv.new_tcp()
				passive:bind(host, port)
				passive:listen(backlog, function (err)
					local stream = uv.new_tcp()
					passive:accept(stream)
					local missing = msgcount*msgsize
					stream:read_start(function (err, chunk)
						missing = missing-#chunk
						if missing <= 0 then
							stream:read_stop()
							stream:close()
							passive:close()
						end
					end)
				end)

				local stream = uv.new_tcp()
				local missing = msgcount
				local function onwrite(err)
					missing = missing-1
					if missing > 0 then
						stream:write(msgdata, onwrite)
					else
						stream:close()
					end
				end
				stream:connect(host, port, function (err)
					stream:write(msgdata, onwrite)
				end)
			]],
			test = [[uv.run()]],
		},
	},
}, {
	id = "idle",
	cases = {
		coutil = {
			lua = os.getenv("LUA54_BIN"),
			gettime = coutiltime,
			setup = [[
				local system = require "coutil.system"
				local spawn = require "coutil.spawn"

				]]..idlecfg..[[

				spawn.catch(print, function ()
					for i = 1, repeats do
						system.suspend()
					end
				end)
			]],
			test = [[system.run()]],
		},
		luv = {
			lua = os.getenv("LUA53_BIN"),
			gettime = uvtime,
			setup = [[
				local uv = require "luv"

				]]..idlecfg:gsub("<const>", "")..[[

				local timer = uv.new_idle()
				timer:start(function ()
					repeats = repeats-1
					if repeats == 0 then
						timer:stop()
					end
				end)
			]],
			test = [[uv.run()]],
		},
		luv_and_coro = {
			lua = os.getenv("LUA53_BIN"),
			gettime = uvtime,
			setup = [[
				local uv = require "luv"

				]]..idlecfg:gsub("<const>", "")..[[

				local yield = coroutine.yield
				local counter = coroutine.create(function ()
					for i = 1, repeats do
						yield(true)
					end
				end)

				local resume = coroutine.resume
				local timer = uv.new_idle()
				timer:start(function ()
					local _, cont = resume(counter)
					if not cont then
						timer:stop()
					end
				end)
			]],
			test = [[uv.run()]],
		},
	},
}
