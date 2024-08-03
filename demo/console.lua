local _G = require "_G"
local load = _G.load
local loadfile = _G.loadfile
local select = _G.select
local xpcall = _G.xpcall

local debug = require "debug"
local getinfo = debug.getinfo
local getregistry = debug.getregistry
local traceback = debug.traceback

local io = require "io"
local stderr = io.stderr

local math = require "math"
local inf = math.huge

local string = require "string"
local format = string.format

local table = require "table"
local concat = table.concat
local insert = table.insert
local unpack = table.unpack

local os = require "os"
local getenv = os.getenv

local memory = require "memory"
local alloc = memory.create

local spawn = require "coutil.spawn"
local catch = spawn.catch

local system = require "coutil.system"
local stdin = system.stdin
local stdout = system.stdout
local runall = system.run

_G._G = _G
_G.debug = debug
_G.io = io
_G.math = math
_G.string = string
_G.table = table
_G.memory = memory
_G.system = system
_G.spawn = spawn

local function writeto(file, ...)
	for i = 1, select("#", ...) do
		file:write((select(i, ...)))
	end
end

local function traceerr(errmsg, level)
	return traceback(errmsg, level or 2)
end

local function catcherr(errmsg)
	writeto(stderr, traceerr(errmsg, 3), "\n")
	return errmsg
end

function spawn.call(f, ...)
	return catch(catcherr, f, ...)
end

local buffer<const> = alloc(512)
local EOFMARK<const> = "<eof>"

local function loadline()
	writeto(stdout, _PROMPT or "> ")
	local result, errmsg = stdin:read(buffer)
	if not result then
		return false, errmsg
	end
	local line = buffer:tostring(1, result)
	result, errmsg = load("return "..line)
	while not result do
		result, errmsg = load(line)
		if not result and errmsg:sub(-#EOFMARK) == EOFMARK then
			writeto(stdout, _PROMPT2 or ">> ")
			result, errmsg = stdin:read(buffer)
			if not result then  -- no input
				return false, errmsg
			end
			line = line.."\n"..buffer:tostring(1, result)
			result = false
		end
	end
	return result
end

local function handleresults(ok, ...)
	if not ok then
		writeto(stderr, (...), "\n")
	elseif select("#", ...) > 0 then
		print(...)
	end
	return ok
end

local function doREPL()
	while true do
		local result, errmsg = loadline()
		if result then
			handleresults(xpcall(result, traceerr))
		elseif not errmsg == "end of file" then
			writeto(stderr, errmsg, "\n")
		else
			break
		end
	end
end

local function dochunk(argv, result, errmsg)
	if result then
		return handleresults(xpcall(result, traceerr, unpack(argv or {})))
	end
	writeto(stderr, errmsg, "\n")
	return false
end

local progname = "lua"
local usage = [=[
usage: %s [options] [script [args]]
Available options are:
  -e stat   execute string 'stat'
  -i        enter interactive mode after executing 'script'
  -l mod    require library 'mod' into global 'mod'
  -l g=mod  require library 'mod' into global 'g'
  -v        show version information
  -E        ignore environment variables
  -W        turn warnings on
  --        stop handling options
  -         stop handling options and execute stdin
]=]

local function printusage(message)
	if message then
		writeto(stderr, progname, ": ", message, "\n");
	end
	writeto(stderr, usage:format(progname));
end

local function printversion()
	writeto(stdout, _VERSION, "  Copyright (C) 1994-2024 Lua.org, PUC-Rio", "\n")
end

local coroutine = require "coroutine"
_DBG_MAIN_THREAD = coroutine.running()

spawn.call(function (...)

	if arg then
		for i = 0, -inf, -1 do
			if not arg[i] then
				break
			end
			progname = arg[i]
		end
	else
		progname = getinfo(1, "S").source
		if progname:sub(1, 1) == "@" then
			progname = progname:sub(2)
		end
		arg = { [0] = progname, ... }
	end

	local flags = { i = false, v = false, E = false, ["-"] = false }
	local options = {}
	local values = {}
	local argv = arg
	local argc = #argv
	local argi = 1
	while argi <= argc and type(argv[argi]) == "string" do
		local option = argv[argi]:match("^%-(.?)$")
		if option == nil or option == "" then
			break
		elseif flags[option] ~= nil then
			flags[option] = true
			if option == "-" then
				break
			end
		elseif option == "l" or option == "e" then
			argi = argi+1
			if argi > argc then
				return printusage(format("'%s' needs argument", option))
			end
			insert(options, option)
			insert(values, argv[argi])
		elseif option == "W" then
			insert(options, option)
			insert(values, false)
		else
			return printusage(format("unrecognized option '%s'", option))
		end
		argi = argi+1
	end

	arg = {}
	for i = argc, -inf, -1 do
		if not argv[i] then
			break
		end
		arg[i-argi] = argv[i]
		progname = argv[i]
	end

	if flags.v then
		printversion()
	end

	if flags.E then
		getregistry().LUA_NOENV = true
	else
		local name = _VERSION:gsub("Lua (%d+).(%d+)", "LUA_INIT_%1_%2")
		local luainit = getenv(name)
		if not luainit then
			name = "LUA_INIT"
			luainit = getenv(name)
		end
		if luainit then
			if luainit:sub(1, 1) == "@" then
				luainit = dochunk(nil, loadfile(luainit:sub(2)))
			else
				luainit = dochunk(nil, load(luainit, "="..name))
			end
			if not luainit then
				return
			end
		end
	end

	for index, value in ipairs(values) do
		if options[index] == 'W' then
			warn("@on")
		elseif options[index] == 'l' then
			local global, modname = value:match("^(.+)=(.+)$")
			if global then
				_G[global] = require(modname)
			else
				require(value)
			end
		elseif not dochunk(nil, load(value, "=(command line)")) then
			return
		end
	end

	if argi <= argc then
		if not dochunk(arg, loadfile(argv[argi])) then
			return
		end
	end

	if flags.i then
		doREPL()
	elseif argi > argc and not concat(options):find("e") and not flags.v then
		if stdin.winsize then
			printversion()
			doREPL()
		else
			dochunk(nil, loadfile())
		end
	end
end, ...)

runall()

--[============================================================================[
spawn.call(function ()
	for i = 9, 0, -1 do
		system.suspend(1)
		system.stderr:send(i)
	end
end)

local threads = require "coutil.threads"
local pool = threads.create(0)
for i = 1, 3 do
	pool:dostring([[
		local _G = require "_G"
		local coroutine = require "coroutine"
		local system = require "coutil.system"
		assert(coroutine.resume(coroutine.create(function ()
			for i = 9, 0, -1 do
				system.suspend(1)
				system.stderr:send(i)
			end
		end)))
		system.run()
	]])
end
pool:resize(3)

print "Hello, World!"
--]============================================================================]
