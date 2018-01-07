package.path = "../lua/?.lua;"..package.path

garbage = setmetatable({ thread = nil }, { __mode = "v" })

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
	collectgarbage("collect")
	assert(next(garbage) == nil)
	print("OK")
end

function asserterr(expected, ok, ...)
	local actual = ...
	assert(ok == false, "error was expected")
	assert(string.find(actual, expected, 1, true), "wrong error, got "..actual)
end

local function errorreporter(errmsg)
	io.stderr:write(debug.traceback(errmsg), "\n")
	return errmsg
end

function spawn(f, ...)
	--[[
	local t = coroutine.create(function (...)
		return select(2, assert(xpcall(f, errorreporter, ...)))
	end)
	--[=[--]]
	local t = coroutine.create(f)
	--]=]
	garbage[#garbage+1] = t
	return coroutine.resume(t, ...)
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

print "\nSuceess!\n"
