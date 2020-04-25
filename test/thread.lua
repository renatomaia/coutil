local system = require "coutil.system"

local protectcode = [[
	local function self(...) %s end
	local _G = require "_G"
	local debug = require "debug"
	local ok, err = _G.xpcall(self, debug.traceback, ...)
	if not ok then
		local io = require "io"
		io.stderr:write(err)
		io.stderr:flush()
	end
]]

local function waitsignal(path, yield)
	local _G = require "_G"
	local io = require "io"
	local os = require "os"
	if yield then
		yield = require("coroutine").yield
	else
		yield = function () end
	end
	repeat
		assert(_G.select("#", yield(1, 2, 3)) == 0)
		local contents
		local file = io.open(path)
		if file then
			contents = file:read()
			file:close()
		end
	until contents == path
	os.remove(path)
end

local function sendsignal(path)
	local io = require "io"
	local file = io.open(path, "w")
	file:write(path)
	file:close()
	while true do
		file = io.open(path)
		if file == nil then break end
		file:close()
	end
end

local waitscript = os.tmpname()
local file = io.open(waitscript, "w")
local main = string.format('require("_G") assert(load(%q, nil, "b"))(...)', string.dump(waitsignal))
file:write(protectcode:format(main))
file:close()

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
	local t = assert(system.threads(1))
	assert(t:dostring[[
		require "_G"
		error "Oops!"
	]] == true)
	repeat until (t:count("tasks") == 0)
	assert(t:close() == true)

	done()
end

do case "type errors"
	local t = assert(system.threads(1))
	asserterr("bad argument #5 (illegal type)",
		t:dostring("", nil, "t", table))
	asserterr("bad argument #6 (illegal type)",
		t:dostring("", nil, "t", 1, print))
	asserterr("bad argument #7 (illegal type)",
		t:dostring("", nil, "t", 1, 2, coroutine.running()))
	asserterr("bad argument #8 (illegal type)",
		t:dostring("", nil, "t", 1, 2, 3, io.stdout))

	done()
end

do case "not contained"
	assert(system.threads() == nil)

	done()
end

do case "no threads"
	local t = assert(system.threads(-1))
	assert(t:count("size") == 0)
	assert(t:count("threads") == 0)
	assert(t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)
	assert(t:close() == true)

	local t = assert(system.threads(0))
	assert(t:count("size") == 0)
	assert(t:count("threads") == 0)
	assert(t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)

	assert(t:dostring[[repeat until false]] == true)
	assert(t:count("size") == 0)
	assert(t:count("threads") == 0)
	assert(t:count("tasks") == 1)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 1)

	assert(t:close() == true)

	done()
end

do case "no tasks"
	local t = assert(system.threads(3))
	assert(t:count("size") == 3)
	assert(t:count("threads") == 3)
	assert(t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)

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

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == path.n)
	repeat until (t:count("running") == 1)
	assert(t:count("pending") == path.n-1)

	while path.n > 0 do
		sendsignal(path[path.n])
		path.n = path.n-1
		assert(t:count("size") == 1)
		assert(t:count("threads") == 1)
		repeat until (t:count("tasks") == path.n)
		if path.n > 0 then
			assert(t:count("running") == 1)
			assert(t:count("pending") == path.n-1)
		else
			assert(t:count("running") == 0)
			assert(t:count("pending") == 0)
		end
	end

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)
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

	assert(t:count("size") == n)
	assert(t:count("threads") == n)
	assert(t:count("tasks") == m)
	repeat until (t:count("running") == n)
	assert(t:count("pending") == m-n)

	for _, path in ipairs(path) do
		os.remove(path)
	end

	repeat until (t:count("tasks") == 0)
	assert(t:count("size") == n)
	assert(t:count("threads") == n)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)
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
	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == 1)
	repeat until (t:count("running") == 1)
	assert(t:count("pending") == 0)

	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)
	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == 2)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 1)

	sendsignal(path1)
	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 1)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 0)

	sendsignal(path2)
	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)

	assert(t:close() == true)

	done()
end

do case "idle threads"
	local t = assert(system.threads(2))
	assert(t:resize(4) == true)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)

	local path3 = os.tmpname()
	assert(t:dofile(waitscript, "t", path3) == true)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 1)
	repeat until (t:count("running") == 1)
	assert(t:count("pending") == 0)

	local path4 = os.tmpname()
	assert(t:dofile(waitscript, "t", path4) == true)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 2)
	repeat until (t:count("running") == 2)
	assert(t:count("pending") == 0)

	sendsignal(path3)
	sendsignal(path4)
	repeat until (t:count("running") == 0)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 0)

	assert(t:close() == true)

	done()
end

do case "increase size"
	local t = assert(system.threads(1))
	local path1 = os.tmpname()
	assert(t:dofile(waitscript, "t", path1) == true)
	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == 2)
	repeat until (t:count("running") == 1)
	assert(t:count("pending") == 1)

	assert(t:resize(4) == true)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 2)
	repeat until (t:count("running") == 2)
	assert(t:count("pending") == 0)

	sendsignal(path1)
	sendsignal(path2)
	repeat until (t:count("running") == 0)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 0)
	assert(t:count("pending") == 0)

	assert(t:close() == true)

	done()
end

do case "decrease size"
	local t = assert(system.threads(4))
	local path1 = os.tmpname()
	assert(t:dofile(waitscript, "t", path1) == true)
	local path2 = os.tmpname()
	assert(t:dofile(waitscript, "t", path2) == true)
	repeat until (t:count("running") == 2)
	assert(t:count("size") == 4)
	assert(t:count("threads") == 4)
	assert(t:count("tasks") == 2)
	assert(t:count("running") == 2)
	assert(t:count("pending") == 0)

	assert(t:resize(1) == true)
	assert(t:count("size") == 1)
	repeat until (t:count("threads") == 2)
	assert(t:count("tasks") == 2)
	assert(t:count("running") == 2)
	assert(t:count("pending") == 0)

	assert(t:dostring[[repeat until false]] == true)
	assert(t:count("size") == 1)
	assert(t:count("threads") == 2)
	assert(t:count("tasks") == 3)
	assert(t:count("running") == 2)
	assert(t:count("pending") == 1)

	sendsignal(path1)
	assert(t:count("size") == 1)
	repeat until (t:count("threads") == 1)
	assert(t:count("tasks") == 2)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 1)

	assert(t:resize(0) == true)
	assert(t:count("size") == 0)
	assert(t:count("threads") == 1)
	assert(t:count("tasks") == 2)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 1)

	sendsignal(path2)
	assert(t:count("size") == 0)
	repeat until (t:count("threads") == 0)
	assert(t:count("tasks") == 1)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 1)

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
		repeat until (t:count("running") == 1)
	end

	gc()
	sendsignal(path1)

	done()
end

local testutils = string.format([[
	local _G = require "_G"
	local waitsignal = _G.load(%q, nil, "b")
	local sendsignal = _G.load(%q, nil, "b")
]], string.dump(waitsignal), string.dump(sendsignal))

do case "nested task"
	local path1 = os.tmpname()
	local path2 = os.tmpname()

	local t = assert(system.threads(1))
	assert(t:dostring(protectcode:format(testutils..string.format([[
		local system = require "coutil.system"
		local t = assert(system.threads())
		assert(t:count("size") == 1)
		assert(t:count("threads") == 1)
		assert(t:count("tasks") == 1)
		assert(t:count("running") == 1)
		assert(t:count("pending") == 0)

		assert(t:dofile(%q, "t", %q))
		waitsignal(%q)
	]], waitscript, path2, path1)), "@nestedtask.lua"))

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 2)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 1)

	sendsignal(path1)

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 1)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 0)

	sendsignal(path2)

	assert(t:close() == true)

	done()
end

do case "nested pool"
	local path1 = os.tmpname()
	local path2 = os.tmpname()

	local t = assert(system.threads(1))
	assert(t:dostring(protectcode:format(testutils..string.format([[
		local system = require "coutil.system"
		local t = assert(system.threads(1))
		assert(t:count("size") == 1)
		assert(t:count("threads") == 1)
		assert(t:count("tasks") == 0)
		assert(t:count("running") == 0)
		assert(t:count("pending") == 0)

		waitsignal(%q)
		assert(t:dofile(%q, "t", %q))
	]], path1, waitscript, path2)), "@nestedpool.lua"))

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 1)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 0)

	sendsignal(path1)

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 1)
	assert(t:count("running") == 1)
	assert(t:count("pending") == 0)

	sendsignal(path2)

	assert(t:count("size") == 1)
	assert(t:count("threads") == 1)
	repeat until (t:count("tasks") == 0)
	assert(t:count("running") == 0)
	assert(t:count("pending") == 0)

	assert(t:close() == true)

	done()
end
