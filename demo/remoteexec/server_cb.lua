dofile "utils.lua"

local uv = require "luv"

local function unpackcmd(chunk)
	local options = { args = {} }
	local execfile = nil
	local size, index = string.unpack(szfmt, chunk)
	execfile, index = string.unpack("z", chunk, index)
	for i = 1, size do
		options.args[i], index = string.unpack("z", chunk, index)
	end
	return execfile, options
end

local function endconn(resources, result)
	log("client request complete (res=",result,")\n")
	resources.conn:write(string.pack("s"..szlen, result))
	resources.conn:shutdown()
	resources.conn:close()
	if resources.timer then
		resources.timer:close()
		resources.timer = nil
	elseif resources.proc then
		uv.kill(resources.pid, "sigint")
		resources.proc:close()
		resources.proc = nil
		resources.pid = nil
	end
end

local function readpayload(resources, chunk)
	local ok, execfile, options = pcall(unpackcmd, chunk)
	if ok then
		log("executing command ",execfile,"\n")
		resources.proc, resources.pid = uv.spawn(execfile, options, function (err, exit, signal)
			resources.proc, resources.pid = nil, nil
			local result = err
			if not err then
				result = string.format("exit=%d signal=%d", exit, signal)
			end
			endconn(resources, result)
		end)
	elseif string.find(execfile, "data too short", 1, "noregex") ~= nil then
		endconn(resources, "malformed message")
	else
		endconn(resources, "server internal error")
		error(execfile)
	end
end

local function readsize(resources, chunk)
	local size, index = string.unpack(szfmt, chunk)
	chunk = string.sub(chunk, index)
	return readpayload, { chunk }, size-#chunk
end

local function doconn(conn)
	log("new client request\n")
	local resources = { conn = conn }
	local buffer = {}
	local missing = szlen
	local action = readsize
	local ok, err = conn:read_start(function (err, chunk)
		assert(not err, err)
		table.insert(buffer, chunk)
		missing = missing-#chunk
		while missing <= 0 do
			chunk = table.concat(buffer)
			action, buffer, missing = action(resources, chunk)
			if action == nil then
				conn:read_stop()
				break
			end
		end
	end)
	if ok then
		resources.timer = uv.new_timer()
		resources.timer:start(maxtime*1000, 0, function ()
			resources.timer:close()
			resources.timer = nil
			conn:read_stop()
			endconn(resources, "timeout")
		end)
	else
		endconn(resources, err)
	end
end

log("starting server ... ")
local flags = {
	socktype = "stream",
	family = "inet6",
}
assert(uv.getaddrinfo(host, port, flags, function (err, addresses)
	assert(not err, err)
	local address = assert(addresses[1])
	local server = assert(uv.new_tcp())
	assert(server:bind(address.addr, address.port))
	assert(server:listen(32, function (err)
		assert(not err, err)
		local conn = assert(uv.new_tcp())
		assert(server:accept(conn))
		doconn(conn)
	end))
	local signal = uv.new_signal()
	signal:start("sigint", function ()
		signal:close()
		server:close()
		log("terminating server ... ")
	end)
	log("started\n")
end))

uv.run()

log("terminated\n")
