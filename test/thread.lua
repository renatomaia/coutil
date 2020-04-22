local system = require "coutil.system"

local waitsignal = os.tmpname()
local file = io.open(waitsignal, "w")
file:write[[
	local _G = require "_G"
	local debug = require "debug"
	local io = require "io"
	local os = require "os"
	local path = ...
	local ok, err = xpcall(function ()
		repeat
			local contents
			local file = io.open(path)
			if file then
				contents = file:read()
				file:close()
			end
		until contents == path
		os.remove(path)
	end, debug.traceback)
	if not ok then
		io.stderr:write(err)
	end
]]
file:close()

local function sendignal(path)
	local file = io.open(path, "w")
	file:write(path)
	file:close()
	while true do
		file = io.open(path)
		if file == nil then break end
		file:close()
	end
end

local function waituntil(cond)
	cond()
end

newtest "threads" --------------------------------------------------------------

assert(system.threads() == nil)

local t = assert(system.threads(1))
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 0)
assert(t:getcount("tasks") == 0)
assert(t:getcount("running") == 0)
assert(t:getcount("pending") == 0)

local path1 = os.tmpname()
assert(t:dofile(waitsignal, "t", path1) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 1)
assert(t:getcount("tasks") == 1)
repeat until (t:getcount("running") == 1)
assert(t:getcount("pending") == 0)

local path2 = os.tmpname()
assert(t:dofile(waitsignal, "t", path2) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 1)
assert(t:getcount("tasks") == 2)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 1)

sendignal(path1)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 1)
repeat until (t:getcount("tasks") == 1)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 0)

assert(t:dofile(waitsignal, "t", path1) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 1)
repeat until (t:getcount("tasks") == 2)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 1)

assert(t:resize(4) == true)
assert(t:getcount("size") == 4)
assert(t:getcount("thread") == 2)
assert(t:getcount("tasks") == 2)
repeat until (t:getcount("running") == 2)
assert(t:getcount("pending") == 0)

assert(t:resize(1) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 2)
assert(t:getcount("tasks") == 2)
assert(t:getcount("running") == 2)
assert(t:getcount("pending") == 0)

local path3 = os.tmpname()
assert(t:dofile(waitsignal, "t", path3) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 2)
assert(t:getcount("tasks") == 3)
assert(t:getcount("running") == 2)
assert(t:getcount("pending") == 1)

sendignal(path1)
assert(t:getcount("size") == 1)
repeat until (t:getcount("thread") == 1)
assert(t:getcount("tasks") == 2)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 1)

assert(t:resize(0) == true)
assert(t:getcount("size") == 0)
assert(t:getcount("thread") == 1)
assert(t:getcount("tasks") == 2)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 1)

sendignal(path2)
assert(t:getcount("size") == 0)
repeat until (t:getcount("thread") == 0)
assert(t:getcount("tasks") == 1)
assert(t:getcount("running") == 0)
assert(t:getcount("pending") == 1)

assert(t:close() == true)

do return end


local path3 = os.tmpname()
assert(t:dofile(waitsignal, "t", path3) == true)
assert(t:getcount("size") == 1)
assert(t:getcount("thread") == 1)
assert(t:getcount("tasks") == 2)
assert(t:getcount("running") == 1)
assert(t:getcount("pending") == 1)

do case "error messages"
	asserterr(system.tpool() == nil)
	for v in ipairs(types) do
		if type(v) ~= "number" then
			asserterr("number expected", pcall(system.tpool, v))
		end
	end
	asserterr("integer expected", pcall(system.tpool, math.pi))
	asserterr("invalid size", pcall(system.tpool, -1))

	done()
end

do case "zero sized"
	local tpool = assert(system.tpool(0))
	tpool:dostring("")
	tpool:dostring("")
	tpool:dostring("")

	local defined = tpool:getcount("size")
	local actual = tpool:getcount("thread")
	local pending = tpool:getcount("pending")

	os.execute("sleep 1")
	assert(io.open(path) == nil)

	tpool:resize(1)
	waitsynced(path)

	done()
end

do case "zero sized"
	local tpool = assert(system.tpool(0))
	local path = startsynced(tpool, "")

	os.execute("sleep 1")
	assert(io.open(path) == nil)

	tpool:resize(1)
	waitsynced(path)

	done()
end

do case "zero sized"
	asserterr("string or memory expected", pcall(system.thread))
	asserterr("string or memory expected", pcall(system.thread, function () end))
	asserterr("string expected", pcall(system.thread, "", function () end))
	asserterr("string expected", pcall(system.thread, "", "empty", function () end))
	--asserterr("invalid argument", pcall(system.thread, "", "empty", "t", function () end))
	--asserterr("invalid argument", pcall(system.thread, "", nil, nil, function () end))
	asserterr("syntax error", system.thread("invalid chunk"))

	done()
end

do case "ignore runtime errors"
	assert(system.thread[[ error "Oops!" ]] == true)

	--done()
end

do return end

do case "parallel execution"
	local path = os.tmpname()
	os.remove(path)

	assert(io.open(path) == nil)
	assert(system.thread(string.format([[
		local io = require "io"
		local os = require "os"
		local path = %q
		while true do
			local file = io.open(path)
			if file ~= nil then
				local contents = file:read("a")
				file:close()
				if contents  == "deleteme" then
					os.remove(path)
					break
				end
			end
		end
	]], path)) == true)

	local file = assert(io.open(path, "w"))
	file:write("deleteme")
	file:close()

	local removed = false
	for i=1, 1e3 do
		if io.open(path) == nil then
			removed = true
			break
		end
	end

	assert(removed)

	done()
end


local function startsynced(chunk, ...)
	local path = os.tmpname()
	os.remove(path)

	assert(io.open(path) == nil)
	assert(system.thread(string.format([[%s
		local io = require "io"
		local os = require "os"
		local path = %q
		local file = io.open(path, "w")
		file:write("OK")
		file:close()
	]], chunk, path), ...) == true)

	return path
end

local function waitsynced(path)
	local result
	for i=1, 1e3 do
		result = readfrom(path)
		if result ~= nil then
			break
		end
	end

	assert(result == "OK")
end

do case "arguments"
	waitsynced(startsynced("assert(... == nil)"))
	waitsynced(startsynced("assert(... == nil)", nil))
	waitsynced(startsynced("assert(... == false)", false))
	waitsynced(startsynced("assert(... == true)", true))
	waitsynced(startsynced("assert(... == 0)", 0))
	waitsynced(startsynced("assert(... == 1)", 1))
	waitsynced(startsynced("assert(... == -1)", -1))
	waitsynced(startsynced("assert(... == 0xadap-16)", 0xadap-16))
	waitsynced(startsynced("assert(... == '')", ""))
	waitsynced(startsynced("assert(... == 'Lua 5.4')", "Lua 5.4"))
	waitsynced(startsynced("assert(... == '\\0')", "\0"))
	waitsynced(startsynced([[
		local a,b,c,d,e = ...
		assert(a == nil)
		assert(b == false)
		assert(c == 123)
		assert(d == 0xfacep-8)
		assert(e == "\001")
	]]), nil, false, 123, 0xfacep-8, "\001")

	done()
end

do case "yield"
	local synced = startsynced[[
		for i = 1, 10 do
			coroutine.yield()
		end
	]]

	waitsynced(synced)

	done()
end
