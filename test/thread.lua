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

do case "reset channel"
	local chunks = {
		function (t, ...)
			local chunk = utilschunk..[[
				local name, path = ...
				local _ENV = require "_G"
				local coroutine = require("coroutine")
				local name = tostring(coroutine.running())
				sendsignal(path)
				local res, errmsg = coroutine.yield(name)
				assert(res == nil)
				assert(errmsg == "channel reset")
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (t, ...)
			local chunk = utilschunk..[[
				local name, path = ...
				local _ENV = require "_G"
				local system = require "coutil.system"
				spawn(function ()
					local coroutine = require("coroutine")
					local name = tostring(coroutine.running())
					local channel = system.channel(name)
					sendsignal(path)
					local res, errmsg = channel:await()
					assert(res == nil)
					assert(errmsg == "channel reset")
					channel:close()
					sendsignal(path)
				end)
				system.run()
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (_, name, path)
			spawn(function ()
				local name = tostring(coroutine.running())
				local channel = system.channel(name)
				sendsignal(path)
				local res, errmsg = channel:await()
				assert(res == nil)
				assert(errmsg == "channel reset")
				channel:close()
				sendsignal(path)
			end)
		end,
	}

	local completed
	spawn(function ()
		local t = assert(system.threads(1))

		for _, chunk in ipairs(chunks) do
			local path = os.tmpname()

			local names = system.channelnames("list")
			assert(next(names) == nil)

			chunk(t, name, path)

			waitsignal(path, system.suspend)
			local names = system.channelnames("reset")
			local key, value = next(names)
			assert(string.match(key, "thread: 0x%x+") ~= nil)
			assert(value == true)
			assert(next(names, key) == nil)
			waitsignal(path, system.suspend)
			system.channelnames("reset")
		end

		assert(t:close())

		completed = true
	end)
	system.run()
	assert(completed == true)

	done()
end

do case "queueing on endpoints"
	local chunks = {
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
		function (t, ...)
			local chunk = utilschunk..[[
				local name, endpoint, path, producer = ...
				local _ENV = require "_G" 
				local system = require "coutil.system"
				spawn(function ()
					local channel = system.channel(name)
					if producer then
						local res, errmsg = channel:sync(endpoint)
						assert(res == nil)
						assert(errmsg == "no match")
						sendsignal(path)
					end
					assert(channel:await(endpoint) == true)
					sendsignal(path)
				end)
				system.run()
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (_, name, endpoint, path, producer)
			spawn(function ()
				local channel = system.channel(name)
				if producer then
					local res, errmsg = channel:sync(endpoint)
					assert(res == nil)
					assert(errmsg == "no match")
					sendsignal(path)
				end
				assert(channel:await(endpoint) == true)
				sendsignal(path)
			end)
		end,
	}

	local completed
	spawn(function ()
		for _, case in pairs{
			{ n = 3, e1 = "i", e2 = "o" },
			{ n = 3, e1 = "o", e2 = "i" },
			{ n = 3, e1 = "in", e2 = "any" },
			{ n = 3, e1 = "out", e2 = "any" },
			{ n = 1, e1 = "any", e2 = "any" },
			{ n = 1, e1 = nil, e2 = nil },
			{ n = 1, e1 = 1, e2 = 2 },
		} do
			local n, e1, e2 = case.n, case.e1, case.e2
			local t = assert(system.threads(2*n))
			system.channelnames("reset")

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
		local chunk = { [1] = [[
			assert(select("#", ...) == ]]..(#expected+1)..[[)
			assert(select(1, ...) == true)
		]] }
		for i, value in ipairs(expected) do
			table.insert(chunk, "assert(select("..(i+1)..", ...) == "..value..")")
		end
		return table.concat(chunk, "\n")
	end

	local chunks = {
		function (t, args, rets, ...)
			local chunk = chunkprefix..[[
				local function assertvalues(...) ]]..assertvalues(rets)..[[ end
				assertvalues(coroutine.yield(name]]..makeargvals(args, ", ")..[[))
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (t, args, rets, ...)
			local chunk = chunkprefix..[[
				local system = require "coutil.system"
				spawn(function ()
					local function assertvalues(...) ]]..assertvalues(rets)..[[ end
					assertvalues(system.channel(name):await(]]..makeargvals(args)..[[))
					sendsignal(path)
				end)
				system.run()
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
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
	local chunks = {
		function (t, args, errmsg, ...)
			local chunk = chunkprefix..[[
				asserterr("]]..errmsg..[[", require("coroutine").yield(name, nil, ]]..args..[[))
				sendsignal(path)
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
		function (t, args, errmsg, ...)
			local chunk = chunkprefix..[[
				local system = require "coutil.system"
				spawn(function ()
					local channel = system.channel(name)
					asserterr("]]..errmsg..[[", pcall(channel.await, channel, nil, ]]..args..[[))
					sendsignal(path)
				end)
				system.run()
			]]
			assert(t:dostring(chunk, "@chunk", "t", ...))
		end,
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
		system.channelnames("reset")
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
