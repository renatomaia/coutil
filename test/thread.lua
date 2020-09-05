local system = require "coutil.system"

local waitscript = os.tmpname()
do
	local file = io.open(waitscript, "w")
	local main = string.format([[
		local path, yield = ...
		if yield then yield = require("coroutine").yield end
		require("_G") assert(load(%q, nil, "b"))(path, yield)
	]], string.dump(waitsignal))
	file:write(main)
	file:close()
end

local LargeArray = {}
function LargeArray:__len()
	return rawget(self, "n") or 200
end
function LargeArray:__index(k)
	return k <= #self and tostring(k) or nil
end

local combine
do
	local function iterate(values, n, current)
		if n == 0 then
			coroutine.yield(current)
		else
			if current == nil then current = {} end
			local pos = #current+1
			for i, value in ipairs(values) do
				current[pos] = value
				iterate(values, n-1, current)
				current[pos] = nil
			end
		end
	end

	function combine(...)
		return coroutine.wrap(iterate), ...
	end
end

newtest "threads" --------------------------------------------------------------

do case "error messages"
	for v in ipairs(types) do
		if type(v) ~= "number" then
			asserterr("number expected", pcall(system.threads, v))
		end
	end
	asserterr("number has no integer representation",
		pcall(system.threads, math.pi))

	done()
end

do case "compilation errors"
	local t = assert(system.threads(0))

	for v in ipairs(types) do
		local ltype = type(v)
		if ltype ~= "string" and ltype ~= "number" then
			asserterr("string or memory expected", pcall(t.dostring, t, v))
			asserterr("string expected", pcall(t.dostring, t, "", v))
			asserterr("string expected", pcall(t.dostring, t, "", "", v))
		end
	end

	local bytecodes = string.dump(function () a = a+1 end)
	asserterr("attempt to load a binary chunk (mode is 't')",
	          t:dostring(bytecodes, nil, "t"))
	asserterr("attempt to load a text chunk (mode is 'b')",
	          t:dostring("a = a+1", "bytecodes", "b"))
	asserterr("syntax error", t:dostring("invalid chunk"))

	done()
end

do case "runtime errors"
	spawn(function ()
		local errpath = os.tmpname()
		local errfile = assert(io.open(errpath, "w"))
		assert(system.execute{
			execfile = luabin,
			arguments = {
				"-e",
				[=[
					local system = require "coutil.system"
					local t = assert(system.threads(1))
					assert(t:dostring([[
						require "_G"
						error "Oops!"
					]], "@chunk.lua") == true)
					assert(t:close())
				]=],
			},
			stderr = errfile,
		})
		assert(errfile:close())
		assert(readfrom(errpath) == "[COUTIL PANIC] chunk.lua:2: Oops!\n")
		os.remove(errpath)
	end)
	system.run()

	done()
end

do case "type errors"
	local t = assert(system.threads(1))
	asserterr("unable to transfer argument #5 (got table)",
		t:dostring("", nil, "t", table))
	asserterr("unable to transfer argument #6 (got function)",
		t:dostring("", nil, "t", 1, print))
	asserterr("unable to transfer argument #7 (got thread)",
		t:dostring("", nil, "t", 1, 2, coroutine.running()))
	asserterr("unable to transfer argument #8 (got userdata)",
		t:dostring("", nil, "t", 1, 2, 3, io.stdout))

	done()
end

do case "couting"
	local t = assert(system.threads(0))
	assert(t:dostring[[repeat until false]] == true)
	assert(t:dostring[[repeat until false]] == true)
	assert(t:dostring[[repeat until false]] == true)

	asserterr("bad option (got 'w')", pcall(function () t:count("wrong") end))

	assert(checkcount(t, "panepane", 3, 0, 3, 0, 3, 0, 3, 0))

	assert(t:close() == true)

	done()
end

do case "not contained"
	assert(system.threads() == nil)

	done()
end

do case "no threads"
	local t = assert(system.threads(-1))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 0, 0))
	assert(t:close() == true)

	local t = assert(system.threads(0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 0, 0))

	assert(t:dostring[[repeat until false]] == true)
	assert(checkcount(t, "nrpsea", 1, 0, 1, 0, 0, 0))

	assert(t:close() == true)

	done()
end

do case "no tasks"
	local t = assert(system.threads(3))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 3, 3))

	assert(t:close() == true)

	done()
end

do case "no arguments"
	local t = assert(system.threads(1))

	local path = os.tmpname()
	local code = string.format([[%s
		assert(select("#", ...) == 0)
		sendsignal(%q)
	]], utilschunk, path)
	assert(t:dostring(code))
	waitsignal(path)

	assert(t:close() == true)

	done()
end

do case "transfer arguments"
	local t = assert(system.threads(1))

	local path = os.tmpname()
	local code = string.format([[%s
		assert(select("#", ...) == 5)
		local a,b,c,d,e = ...
		assert(a == nil)
		assert(b == false)
		assert(c == 123)
		assert(d == 0xfacep-8)
		assert(e == "\001")

		sendsignal(%q)
	]], utilschunk, path)
	assert(t:dostring(code, "@chunk.lua", "t",
	                  nil, false, 123, 0xfacep-8, "\001"))
	waitsignal(path)

	assert(t:close() == true)

	done()
end

do case "yielding tasks"
	local t = assert(system.threads(1))
	local path = { n = 5 }
	for i = 1, path.n do
		path[i] = os.tmpname()
		assert(t:dofile(waitscript, "t", path[i], "yield") == true)
	end

	repeat until (checkcount(t, "r", 1))
	assert(checkcount(t, "nrpsea", path.n, 1, path.n-1, 0, 1, 1))

	while path.n > 0 do
		sendsignal(path[path.n])
		path.n = path.n-1
		repeat until (checkcount(t, "n", path.n))
		if path.n > 0 then
			assert(checkcount(t, "nrpsea", path.n, 1, path.n-1, 0, 1, 1))
		else
			assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 1, 1))
		end
	end

	assert(t:close() == true)

	done()
end

do case "many threads, even more tasks"
	local path = {}
	local m, n = 50, 5
	local t = assert(system.threads(n))
	for i = 1, m do
		path[i] = os.tmpname()
		assert(t:dostring([[
			local _G = require "_G"
			local io = require "io"
			local debug = require "debug"
			local coroutine = require "coroutine"
			local path = ...
			local ok, err = xpcall(function ()
				while true do
					local file = io.open(path)
					if file == nil then break end
					file:close()
					coroutine.yield()
				end
				local file = assert(io.open(path, "w"))
				file:write(path)
				file:close()
			end, debug.traceback)
			if not ok then
				io.stderr:write(err)
				io.stderr:flush()
			end
		]], nil, "t", path[i]) == true)
	end

	assert(checkcount(t, "nea", m, n, n))
	repeat until (checkcount(t, "r", n))
	assert(checkcount(t, "nrpsea", m, n, m-n, 0, n, n))

	for _, path in ipairs(path) do
		os.remove(path)
	end

	repeat until (checkcount(t, "n", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, n, n))
	assert(t:close() == true)

	for _, path in ipairs(path) do
		local file = assert(io.open(path))
		assert(file:read() == path)
		file:close()
		os.remove(path)
	end

	done()
end

do case "pending tasks"
	local t = assert(system.threads(1))
	local path1 = os.tmpname()
	assert(t:dofile(waitscript, "t", path1) == true)
	repeat until (checkcount(t, "r", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))

	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)
	assert(checkcount(t, "nrpsea", 2, 1, 1, 0, 1, 1))

	sendsignal(path1)
	repeat until (checkcount(t, "n", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))

	sendsignal(path2)
	repeat until (checkcount(t, "n", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 1, 1))

	assert(t:close() == true)

	done()
end

do case "idle threads"
	local t = assert(system.threads(2))
	assert(t:resize(4) == true)
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 4, 2))

	local path3 = os.tmpname()
	assert(t:dofile(waitscript, "t", path3) == true)
	repeat until (checkcount(t, "r", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 4, 2))

	local path4 = os.tmpname()
	assert(t:dofile(waitscript, "t", path4) == true)
	repeat until (checkcount(t, "r", 2))
	assert(checkcount(t, "nrpsea", 2, 2, 0, 0, 4, 2))

	sendsignal(path3)
	sendsignal(path4)
	repeat until (checkcount(t, "r", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 4, 2))

	assert(t:close() == true)

	done()
end

do case "increase size"
	local t = assert(system.threads(1))
	local path1 = os.tmpname()
	assert(t:dofile(waitscript, "t", path1) == true)
	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)

	repeat until (checkcount(t, "r", 1))
	assert(checkcount(t, "nrpsea", 2, 1, 1, 0, 1, 1))

	assert(t:resize(4) == true)
	assert(checkcount(t, "e", 4))
	repeat until (checkcount(t, "r", 2))
	assert(checkcount(t, "nrpsea", 2, 2, 0, 0, 4, 2))

	sendsignal(path1)
	sendsignal(path2)
	repeat until (checkcount(t, "r", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 4, 2))

	assert(t:close() == true)

	done()
end

do case "decrease size"
	local t = assert(system.threads(5))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 5, 5))

	assert(t:resize(4) == true)
	assert(checkcount(t, "e", 4))
	repeat until (checkcount(t, "a", 4))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 4, 4))

	local path1 = os.tmpname()
	assert(t:dofile(waitscript, "t", path1) == true)
	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)
	repeat until (checkcount(t, "r", 2))
	assert(checkcount(t, "nrpsea", 2, 2, 0, 0, 4, 4))

	assert(t:resize(1) == true)
	assert(checkcount(t, "e", 1))
	repeat until (checkcount(t, "a", 2))
	assert(checkcount(t, "nrpsea", 2, 2, 0, 0, 1, 2))

	assert(t:dostring[[repeat until false]] == true)
	assert(checkcount(t, "nrpsea", 3, 2, 1, 0, 1, 2))

	sendsignal(path1)
	repeat until (checkcount(t, "a", 1))
	assert(checkcount(t, "nrpsea", 2, 1, 1, 0, 1, 1))

	assert(t:resize(0) == true)
	assert(checkcount(t, "nrpsea", 2, 1, 1, 0, 0, 1))

	sendsignal(path2)
	repeat until (checkcount(t, "a", 0))
	assert(checkcount(t, "nrpsea", 1, 0, 1, 0, 0, 0))

	assert(t:close() == true)

	done()
end

do case "collect pending"
	assert(system.threads(0):dostring[[repeat until false]])

	done()
end

do case "collect running"
	local path1 = os.tmpname()

	do
		local t = assert(system.threads(1))
		assert(t:dofile(waitscript, "t", path1) == true)
		repeat until (checkcount(t, "r", 1))
	end

	gc()
	sendsignal(path1)

	done()
end

do case "nested task"
	local path1 = os.tmpname()
	local path2 = os.tmpname()

	local t = assert(system.threads(1))
	local code = string.format([[%s
		local system = require "coutil.system"
		local t = assert(system.threads())
		assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))
		assert(t:dofile(%q, "t", %q))
		waitsignal(%q)
	]], utilschunk, waitscript, path2, path1)
	assert(t:dostring(code, "@chunk.lua"))

	repeat until (checkcount(t, "n", 2))
	assert(checkcount(t, "nrpsea", 2, 1, 1, 0, 1, 1))

	sendsignal(path1)

	repeat until (checkcount(t, "n", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))

	sendsignal(path2)

	repeat until (checkcount(t, "n", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 1, 1))

	assert(t:close() == true)

	done()
end

do case "nested pool"
	local path1 = os.tmpname()
	local path2 = os.tmpname()

	local t = assert(system.threads(1))
	local code = string.format([[%s
		local system = require "coutil.system"
		local t = assert(system.threads(1))
		assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 1, 1))

		waitsignal(%q)
		assert(t:dofile(%q, "t", %q))
	]], utilschunk, path1, waitscript, path2)
	assert(t:dostring(code, "@chunk.lua"))

	repeat until (checkcount(t, "r", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))

	sendsignal(path1)

	repeat until (checkcount(t, "n", 1))
	assert(checkcount(t, "nrpsea", 1, 1, 0, 0, 1, 1))

	sendsignal(path2)

	repeat until (checkcount(t, "n", 0))
	assert(checkcount(t, "nrpsea", 0, 0, 0, 0, 1, 1))

	assert(t:close() == true)

	done()
end

do case "inherit preload"
	package.preload["coutil.spawn"] =
		assert(package.searchers[2]("coutil.spawn"))
	package.preload["coutil.system"] =
		assert(package.searchers[3]("coutil.system"))

	local path = os.tmpname()
	local t = assert(system.threads(1))
	local code = string.format([[%s
		local package = require "package"
		package.path = ""
		package.cpath = ""

		require "coutil.spawn"
		require "coutil.system"

		sendsignal(%q)
	]], utilschunk, path)
	assert(t:dostring(code, "@chunk.lua"))

	waitsignal(path)

	package.preload["coutil.spawn"] = nil
	package.preload["coutil.system"] = nil

	done()
end

do case "system coroutine"
	spawn(function ()
		local syscoro = system.load[[
			local _G = require "_G"
			local system = require "coutil.system"
			assert(system.threads() == nil)
			return true
		]]
		local ok, res = syscoro:resume()
		assert(ok == true and res == true)
	end)

	system.run()

	done()
end

newtest "channels" --------------------------------------------------------------

do case "errors"
	asserterr("string expected", pcall(system.channel))
	asserterr("table expected", pcall(system.channelnames, "all"))

	done()
end

do case "already in use"
	local channel = system.channel("some channel")

	local a
	spawn(function ()
		a = 1
		channel:await()
		a = 2
	end)
	assert(a == 1)

	local b
	spawn(function ()
		asserterr("in use", pcall(channel.await, channel))
		channel:sync()
		b = 1
	end)

	assert(b == 1)
	system.run()
	assert(a == 2)

	done()
end

do case "scheduled yield"
	local name = tostring{}

	local stage = 0
	spawn(function ()
		assert(system.channel(name):await() == true)
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		assert(system.channel(name):sync() == true)
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule same channel"
	local name = tostring{}

	local stage = 0
	spawn(function ()
		local channel = system.channel(name)
		assert(channel:await() == true)
		stage = 1
		assert(channel:await() == true)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		assert(system.channel(name):sync() == true)
		system.suspend()
		assert(system.channel(name):sync() == true)
	end)

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "reschedule different channels"
	local name1 = tostring{}
	local name2 = tostring{}

	local stage = 0
	spawn(function ()
		assert(system.channel(name1):await() == true)
		stage = 1
		assert(system.channel(name2):await() == true)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		assert(system.channel(name1):sync() == true)
		system.suspend()
		assert(system.channel(name2):sync() == true)
	end)

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local a,b,c = system.channel(tostring{}):await()
		assert(a == true)
		assert(b == nil)
		assert(c == 3)
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro, true,nil,3)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "cancel and reschedule"
	local name = tostring{}

	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local channel = system.channel(name)
		local extra = channel:await()
		assert(extra == nil)
		stage = 1
		assert(channel:await() == true)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend() -- the first await is active.
		system.suspend() -- the first await is being closed.
		assert(system.channel(name):sync() == true)-- the second await is active.
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "resume while closing"
	local name = tostring{}

	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local channel = system.channel(name)
		assert(channel:await() == nil)
		stage = 1
		local a,b,c = channel:await()
		assert(a == .1)
		assert(b == 2.2)
		assert(c == 33.3)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
		assert(stage == 2)
		stage = 3
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 3)

	done()
end

do case "ignore errors"
	local name = tostring{}

	local stage = 0
	pspawn(function ()
		assert(system.channel(name):await() == true)
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		assert(system.channel(name):sync() == true)-- the second await is active.
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		assert(system.channel(tostring{}):await() == garbage)
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro, garbage)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "close channels"
	spawn(function ()
		local channel = system.channel("closing channel")
		assert(channel:close() == true)
		assert(channel:close() == false)
		for _, opname in ipairs{ "await", "sync", "getname" } do
			asserterr("closed channel", pcall(channel[opname], channel))
		end
	end)

	system.run()

	done()
end

do case "collect while in use"
	local t = system.threads(1)
	t:dostring(utilschunk..[[
		local system = require "coutil.system"
		spawn(function () system.channel("c"):await() end)
	]])
	t:close()

	local ok, err = system.channel("c"):sync()
	assert(ok == nil)
	assert(err == "empty")

	dostring(utilschunk..[[
		local system = require "coutil.system"
		spawn(function () system.channel("c"):await() end)
	]])

	done()
end

do case "close while in use"
	local channel = system.channel("c")
	local ended = false
	spawn(function ()
		channel:await()
		ended = true
	end)
	assert(not ended)
	asserterr("in use", pcall(channel.close, channel))
	assert(not ended)
	assert(system.channel("c"):sync() == true)
	assert(not ended)
	assert(system.run() == false)
	assert(ended)

	done()
end

do case "collect suspended"
	local name = tostring{}

	do
		local t = assert(system.threads(1))
		assert(t:dostring([[ require("coroutine").yield("]]..name..[[") ]]))
		repeat until (checkcount(t, "s", 1))
	end

	gc()
	assert(system.channel(name):sync() == true)

	done()
end

do case "suspended without threads"
	local name = tostring{}

	local t = system.threads(1)
	assert(t:dostring([[ require("coroutine").yield("]]..name..[[") ]]))
	repeat until (checkcount(t, "s", 1))
	assert(t:resize(0))
	assert(t:close())

	assert(system.channel(name):sync() == true)

	done()
end

do case "collect channels with tasks"
	dostring([=[
		local system = require "coutil.system"
		local t = system.threads(1)
		assert(t:dostring([[ require("coroutine").yield("channel01") ]]))
		repeat until (t:count("s", 1))
		assert(t:resize(0))
	]=])

	done()
end

do case "queueing on endpoints"
	local body = [[
		local channel = system.channel(name)
		if producer then
			local res, errmsg = channel:sync(endpoint)
			assert(res == nil)
			assert(errmsg == "empty")
			sendsignal(path)
		end
		assert(channel:await(endpoint) == true)
		sendsignal(path)
	]]
	local channelchunk = utilschunk..[[
		local name, endpoint, path, producer = ...
		local _ENV = require "_G"
		local system = require "coutil.system"
		spawn(function ()
			]]..body..[[
		end)
		system.run()
	]]
	local chunks = {
		-- task yield
		function (t, ...)
			local chunk = utilschunk..[[
				local name, endpoint, path, producer = ...
				local _ENV = require "_G"
				if producer then sendsignal(path) end
				assert(require("coroutine").yield(name, endpoint) == true)
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- task
		function (t, ...)
			assert(t:dostring(channelchunk, "@chunk", "t", ...))
		end,
		-- system coroutine
		function (_, ...)
			spawn(function (...)
				local sysco = assert(system.load(channelchunk, "@chunk", "t"))
				assert(sysco:resume(...))
			end, ...)
		end,
		-- coroutine
		function (_, name, endpoint, path, producer)
			local main = assert(load("local system, name, endpoint, path, producer = ... "..body))
			spawn(main, system, name, endpoint, path, producer)
		end,
	}

	local completed
	spawn(function ()
		for _, case in pairs{
			{ n = 2, e1 = "in", e2 = "out" },
			{ n = 2, e1 = "out", e2 = "in" },
			{ n = 2, e1 = "in", e2 = "any" },
			{ n = 2, e1 = "out", e2 = "any" },
			{ n = 1, e1 = "any", e2 = "any" },
			{ n = 1, e1 = nil, e2 = nil },
		} do
			local n, e1, e2 = case.n, case.e1, case.e2
			local t = assert(system.threads(2*n))

			local channel = tostring(case)
			for producer in combine(chunks, n) do
				for consumer in combine(chunks, n) do
					local paths = { producer = {}, consumer = {} }
					for i = 1, n do
						paths.producer[i] = os.tmpname()
						producer[i](t, channel, e1, paths.producer[i], true)
					end
					for i = 1, n do
						waitsignal(paths.producer[i], system.suspend)
					end
					for i = 1, n do
						paths.consumer[i] = os.tmpname()
						consumer[i](t, channel, e2, paths.consumer[i], false)
					end
					for i = 1, n do
						waitsignal(paths.producer[i], system.suspend)
						waitsignal(paths.consumer[i], system.suspend)
					end
				end
			end

			assert(t:close())

			io.write(".")
			io.flush()
		end
		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

do case "transfer values"
	local chunkprefix = utilschunk..[[
		local name, path = ...
		local _ENV = require "_G"
		local coroutine = require "coroutine"
		local table = require "table"
		local string = require "string"
		local math = require "math"
		local io = require "io"
	]]
	local function makeargvals(args, prefix)
		local chunk = ""
		if #args > 0 then
			chunk = "nil , "..table.concat(args, ", ")
			if prefix ~= nil then
				chunk = prefix..chunk
			end
		end
		return chunk
	end
	local function assertvalues(expected)
		local chunk = { [[
			assert(select("#", ...) == ]]..(#expected+1)..[[)
			assert(select(1, ...) == true)
		]] }
		for i, value in ipairs(expected) do
			table.insert(chunk, "assert(select("..(i+1)..", ...) == "..value..")")
		end
		return table.concat(chunk, "\n")
	end
	local function makechannelchunk(args, rets)
		return chunkprefix..[[
			local system = require "coutil.system"
			spawn(function ()
				local function assertvalues(...) ]]..assertvalues(rets)..[[ end
				assertvalues(system.channel(name):await(]]..makeargvals(args)..[[))
				sendsignal(path)
			end)
			system.run()
		]]
	end

	local chunks = {
		-- task yield
		function (t, args, rets, ...)
			local chunk = chunkprefix..[[
				local function assertvalues(...) ]]..assertvalues(rets)..[[ end
				assertvalues(coroutine.yield(name]]..makeargvals(args, ", ")..[[))
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- task
		function (t, args, rets, ...)
			local chunk = makechannelchunk(args, rets)
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- system coroutine
		function (_, args, rets, ...)
			spawn(function (...)
				local chunk = makechannelchunk(args, rets)
				local sysco = assert(system.load(chunk, "@chunk", "t"))
				assert(sysco:resume(...))
			end, ...)
		end,
		-- coroutine
		function (_, args, rets, name, path)
			local makeargvals = assert(load(makeargvals(args, "return ")))
			local assertvalues = assert(load(assertvalues(rets)))
			spawn(function ()
				assertvalues(system.channel(name):await(makeargvals()))
				sendsignal(path)
			end)
		end,
	}

	local completed
	spawn(function ()
		local t = assert(system.threads(2))
		local many = setmetatable({}, LargeArray)
		for _, case in ipairs({
			{ { "nil", "'error message'" }, {} },
			{ {}, { "nil", "false", "nil" } },
			{ { "math.huge", "0xabcdefp-123", "math.pi", "-65536" }, { "''", [[string.rep("\0\1\2\3\4\5", 0x1p10)]], "'\255'", "'CoUtil'" } },
			{ many, many },
			{ many, {} },
			{ {}, many },
		}) do
			for tasks in combine(chunks, 2) do
				local name = tostring(tasks)
				local path1 = os.tmpname()
				local path2 = os.tmpname()
				tasks[1](t, case[1], case[2], name, path1)
				tasks[2](t, case[2], case[1], name, path2)
				waitsignal(path1, system.suspend)
				waitsignal(path2, system.suspend)
			end
		end
		assert(t:close())
		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

do case "transfer errors"
	local chunkprefix = utilschunk..[[
		local name, path = ...
		local _ENV = require "_G"
		local coroutine = require "coroutine"
		local table = require "table"
		local io = require "io"
	]]
	local function makechannelchunk(args, errmsg)
		return chunkprefix..[[
			local system = require "coutil.system"
			spawn(function ()
				local channel = system.channel(name)
				asserterr("]]..errmsg..[[", pcall(channel.await, channel, nil, ]]..args..[[))
				sendsignal(path)
			end)
			system.run()
		]]
	end
	local chunks = {
		-- task yield
		function (t, args, errmsg, ...)
			local chunk = chunkprefix..[[
				asserterr("]]..errmsg..[[", require("coroutine").yield(name, nil, ]]..args..[[))
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- task
		function (t, args, errmsg, ...)
			local chunk = makechannelchunk(args, errmsg)
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- system coroutine
		function (_, args, errmsg, ...)
			local chunk = makechannelchunk(args, errmsg)
			spawn(function (...)
				local sysco = assert(system.load(chunk, "@chunk", "t"))
				assert(sysco:resume(...))
			end, ...)
		end,
		-- coroutine
		function (_, args, errmsg, name, path)
			spawn(function ()
				local makeargvals = assert(load("return "..args))
				local channel = system.channel(name)
				asserterr(errmsg, pcall(channel.await, channel, nil, makeargvals()))
				sendsignal(path)
			end)
		end,
	}
	local many = setmetatable({}, LargeArray)
	many[#many] = "{}"
	local completed
	spawn(function ()
		local t = assert(system.threads(1))
		for _, case in ipairs({
			{ arg = 2, values = { "nil", "coroutine.running()" } },
			{ arg = 4, values = { "nil", "false", "nil", "print" } },
			{ arg = 10, values = { "nil", "nil", "nil", "nil", "nil", "nil", "nil", "nil", "nil", "io.stdout" } },
			{ arg = #many, values = many },
		}) do
			local name = tostring(case)
			local path = os.tmpname()
			local errval = assert(load("return "..case.values[case.arg]))()
			local errmsg = string.format("unable to transfer argument #%d (got %s)",
			                             2+case.arg, type(errval))
			local args = table.concat(case.values, ", ")
			for _, task in ipairs(chunks) do
				task(t, args, errmsg, name, path)
				waitsignal(path, system.suspend)
			end
		end
		assert(t:close())
		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

do case "invalid endpoint"
	local chunkprefix = utilschunk..[[
		local name, path = ...
	]]
	local function makechannelchunk(arg, errmsg)
		return chunkprefix..[[
			local system = require "coutil.system"
			spawn(function ()
				local channel = system.channel(name)
				asserterr("]]..errmsg..[[", pcall(channel.await, channel, "]]..arg..[["))
				sendsignal(path)
			end)
			system.run()
		]]
	end
	local chunks = {
		-- task yield
		function (t, arg, errmsg, ...)
			local chunk = chunkprefix..[[
				asserterr("]]..errmsg..[[", require("coroutine").yield(name, "]]..arg..[["))
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- task
		function (t, arg, errmsg, ...)
			local chunk = makechannelchunk(arg, errmsg)
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		-- system coroutine
		function (_, arg, errmsg, ...)
			local chunk = makechannelchunk(arg, errmsg)
			spawn(function (...)
				local sysco = assert(system.load(chunk))
				assert(sysco:resume(...))
			end, ...)
		end,
		-- coroutine
		function (_, arg, errmsg, name, path)
			spawn(function ()
				local channel = system.channel(name)
				asserterr(errmsg, pcall(channel.await, channel, arg))
				sendsignal(path)
			end)
		end,
	}
	spawn(function ()
		local t = assert(system.threads(1))
		for _, arg in ipairs({ "i", "o", "input", "output", "" }) do
			local errmsg = string.format("bad argument #2 (invalid option '%s')", arg)
			for _, task in ipairs(chunks) do
				local path = os.tmpname()
				task(t, arg, errmsg, tostring({}), path)
				waitsignal(path, system.suspend)
			end
		end
		assert(t:close())
		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

do case "channels names"
	for _, name in ipairs({
		"",
		"\0",
		"\255",
		string.rep("\0\1\2\3\4\5", 0x1p10),
		"CoUtil"
	}) do
		local channel = system.channel(name)
		assert(channel:getname() == name)
		assert(channel:close())
	end

	done()
end

do case "list channels"
	local empty = {}
	local result = system.channelnames(empty)
	assert(result == empty)

	local channels = {}
	for i = 1, 10 do
		local name = tostring(i)
		channels[name] = system.channel(name)
	end
	local names = system.channelnames()
	for i = 1, 10 do
		local name = tostring(i)
		assert(names[name] == true)
		names[name] = nil
	end
	assert(next(names) == nil)

	for i = 1, 10 do
		local name = tostring(i)
		names[name] = name
		if i%2 == 0 then
			channels[name]:close()
		end
	end
	local result = system.channelnames(names)
	assert(result == names)
	for i = 1, 10 do
		local name = tostring(i)
		if i%2 == 0 then
			assert(names[name] == nil)
		else
			assert(names[name] == true)
			channels[name]:close()
		end
		names[name] = nil
	end
	assert(next(names) == nil)

	done()
end

do case "resume listed channels"
	local body = [[
		local name = tostring(coroutine.running())
		local channel = system.channel(name)
		sendsignal(path)
		local res, errmsg = channel:await()
		assert(res == true)
		assert(errmsg == "reset")
		channel:close()
		sendsignal(path)
	]]
	local channelchunk = utilschunk..[[
		local name, path = ...
		local _ENV = require "_G"
		local system = require "coutil.system"
		spawn(function ()
			local coroutine = require("coroutine")
			]]..body..[[
		end)
		system.run()
	]]
	local chunks = {
		function (t, ...)
			local chunk = utilschunk..[[
				local name, path = ...
				local _ENV = require "_G"
				local coroutine = require("coroutine")
				local name = tostring(coroutine.running())
				sendsignal(path)
				local res, errmsg = coroutine.yield(name)
				assert(res == true)
				assert(errmsg == "reset")
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (t, ...)
			assert(t:dostring(channelchunk, "@chunk", "t", ...))
		end,
		function (_, ...)
			spawn(function (...)
				local sysco = assert(system.load(channelchunk, "@chunk", "t"))
				assert(sysco:resume(...))
			end, ...)
		end,
		function (_, name, path)
			local main = assert(load("local system, name, path = ... "..body))
			spawn(main, system, name, path)
		end,
	}

	local completed
	spawn(function ()
		local t = assert(system.threads(1))

		for _, chunk in ipairs(chunks) do
			local path = os.tmpname()

			assert(next(system.channelnames()) == nil)

			chunk(t, name, path)
			waitsignal(path, system.suspend)
			local names = system.channelnames()
			local name, value = next(names)
			assert(next(names, name) == nil)
			assert(string.match(name, "thread: 0x%x+") ~= nil)
			assert(value == true)
			local channel = system.channel(name)
			channel:sync(nil, "reset")
			channel:close()
			waitsignal(path, system.suspend)

			assert(next(system.channelnames()) == nil)
		end

		assert(t:close())

		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

 -- DESTROYS TASK, LEAVES CHANNELS
do case "end with channel left by ended task"
	dostring(utilschunk..[=[
		local system = require "coutil.system"
		local t = system.threads(1)
		t:dostring(utilschunk..[[
			local system = require "coutil.system"
			spawn(function ()
				system.channel("My Channel"):await()
			end)
		]])
		t:close()
	]=])

	done()
end

-- DESTROYS CHANNELS, LEAVES TASK
do case "end with task waiting on channel"
	dostring(utilschunk..[=[
		local system = require "coutil.system"
		local t = system.threads(1)
		t:dostring(utilschunk..[[
			local coroutine = require "coroutine"
			coroutine.yield("My Channel")
		]])
		t:resize(0)
		t:close()
	]=])

	done()
end

-- LEAVES TASK AND CHANNELS
do case "end with task with coroutine waiting on channel"
	dostring(utilschunk..[=[
		local system = require "coutil.system"
		local t = system.threads(1)
		t:dostring(utilschunk..[[
			local system = require "coutil.system"
			spawn(function ()
				system.channel("My Channel"):await()
			end)
			system.run()
		]])
		t:resize(0)
		t:close()
	]=])

	done()
end
