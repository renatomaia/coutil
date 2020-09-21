local _ENV = require "_G"
warn("@on")
local package = require "package"
local table = require "table"
local string = require "string"
local coroutine = require "coroutine"
local io = require "io"
local os = require "os"
local debug = require "debug"

package.path = "../lua/?.lua;"..package.path

local vararg = require "vararg"

garbage = setmetatable({ thread = nil }, { __mode = "v" })

local spawnerr

function gc()
	for i = 1, 3 do collectgarbage("collect") end
end

function asserterr(expected, ok, ...)
	local actual = ...
	assert(not ok, "error was expected, got "..table.concat({vararg.map(tostring, ok, ...)}, ", "))
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
if arg ~= nil then
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

do
	local scriptfile = os.tmpname()
	local successfile = os.tmpname()

	function dostring(chunk, ...)
		writeto(scriptfile, [[
			local function main(...) ]], chunk, [[ end
			local exitval = main(...)
			local file = assert(io.open("]], successfile, [[", "w"))
			assert(file:write("SUCCESS!"))
			assert(file:close())
			os.exit(exitval, true)
		]])
		local command = table.concat({ luabin, scriptfile, ... }, " ")
		local ok, exitmode, exitvalue = os.execute(command)
		assert(ok == true)
		assert(exitmode == "exit")
		assert(exitvalue == 0)
		assert(readfrom(successfile) == "SUCCESS!")
		assert(os.remove(scriptfile))
		os.remove(successfile)
	end
end

function waitsignal(path, yield)
	local _G = require "_G"
	local io = require "io"
	local os = require "os"
	repeat
		if yield ~= nil then yield() end
		local contents
		local file = io.open(path)
		if file then
			contents = file:read()
			file:close()
		end
	until contents == path
	os.remove(path)
end

function sendsignal(path)
	local io = require "io"
	local file = assert(io.open(path, "w"))
	file:write(path)
	file:close()
	--while true do
	--	file = io.open(path)
	--	if file == nil then break end
	--	file:close()
	--end
end

function checkcount(threads, options, ...)
	assert(#options == select("#", ...))
	local results = { threads:count(options) }
	assert(#options == #results)
	for i = 1, #options do
		if results[i] ~= select(i, ...) then
			return false
		end
	end
	return true
end

utilschunk = string.format([[
	local _G = require "_G"
	local waitsignal = _G.load(%q, nil, "b")
	local sendsignal = _G.load(%q, nil, "b")
	local checkcount = _G.load(%q, nil, "b")
	dofile "utils.lua"
]], string.dump(waitsignal), string.dump(sendsignal), string.dump(checkcount))

