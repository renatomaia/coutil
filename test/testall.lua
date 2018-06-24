package.path = "../lua/?.lua;"..package.path

garbage = setmetatable({ thread = nil }, { __mode = "v" })

local spawnerr

local test

function newtest(title)
	test = title
	print()
end

function case(title)
	io.write("[",test,"]",string.rep(" ", 10-#test),title," ... ")
	io.flush()
end

function done()
	assert(spawnerr == nil)
	collectgarbage("collect")
	assert(next(garbage) == nil)
	print("OK")
end

function asserterr(expected, ok, ...)
	local actual = ...
	assert(ok == false, "error was expected")
	assert(string.find(actual, expected, 1, true), "wrong error, got "..actual)
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
		pspawn(xpcall, f, catcherr, ...)
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

dofile "event.lua"
dofile "promise.lua"
dofile "spawn.lua"

print "\nSuceess!\n"
