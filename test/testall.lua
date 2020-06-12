package.path = "../lua/?.lua;"..package.path

local vararg = require "vararg"

garbage = setmetatable({ thread = nil }, { __mode = "v" })

local spawnerr

local test

function newgroup(title)
	print("\n--- "..title.." "..string.rep("-", 65-#title))
end

function newtest(title)
	test = title
	print()
end

function case(title)
	io.write("[",test,"]",string.rep(" ", 10-#test),title," ... ")
	io.flush()
end

function gc()
	for i = 1, 3 do collectgarbage("collect") end
end

function done()
	do -- TODO: workaround for issue in libuv (https://mail.google.com/mail/u/0/#sent/QgrcJHsHqgXlgsCNsmZnpLGLcQtvmzLPFBv)
		local system = require "coutil.system"
		spawn(system.suspend)
		assert(system.run("ready") == false)
		gc()
	end
	assert(spawnerr == nil)
	collectgarbage("collect")
	assert(next(garbage) == nil)
	print("OK")
end

function asserterr(expected, ok, ...)
	local actual = ...
	assert(not ok, "error was expected, got "..table.concat({vararg.map(tostring, ...)}, ", "))
	assert(string.find(actual, expected, 1, true), "wrong error, got "..actual)
end

function assertnone(...)
	assert(select("#", ...) == 0)
end

function pspawn(f, ...)
	local t = coroutine.create(f)
	garbage[#garbage+1] = t
	return coroutine.resume(t, ...)
end

do
	local function catcherr(errmsg)
		if spawnerr == nil then
			spawnerr = debug.traceback(errmsg)
			io.stderr:write(spawnerr, "\n")
		end
		return errmsg
	end

	function spawn(f, ...)
		return select(2, assert(pspawn(xpcall, f, catcherr, ...)))
	end
end

function counter()
	local c = 0
	return function ()
		c = c+1
		return c
	end
end

types = {
	false, true,
	1, 0, -1, .0123,
	"", _VERSION ,"\0",
	{},table,
	function() end,print,
	coroutine.running(),coroutine.create(print),
	io.stdout,
}

luabin = "lua"
do
	local i = -1
	while arg[i] ~= nil do
		luabin = arg[i]
		i = i-1
	end
end

function writeto(path, ...)
	local file = assert(io.open(path, "w"))
	assert(file:write(...))
	assert(file:close())
end

function readfrom(path)
	local file = io.open(path, "r")
	if file ~= nil then
		local data = assert(file:read("*a"))
		assert(file:close())
		return data
	end
end

--dofile "event.lua"
--dofile "queued.lua"
--dofile "promise.lua"
--dofile "mutex.lua"
--dofile "spawn.lua"
--dofile "system.lua"
--dofile "time.lua"
--dofile "signal.lua"
--dofile "netaddr.lua"
--dofile "stream.lua"
--dofile "network.lua"
--dofile "pipe.lua"
--dofile "process.lua"
--dofile "coroutine.lua"
dofile "thread.lua"

print "\nSuccess!\n"
