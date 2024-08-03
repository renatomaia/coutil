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

local os = require "os"
local getenv = os.getenv

local package = require "package"
local config = package.config

local string = require "string"
local format = string.format

local table = require "table"
local concat = table.concat
local insert = table.insert
local unpack = table.unpack

local memory = require "memory"
local alloc = memory.create

local spawn = require "coutil.spawn"
local catch = spawn.catch

local system = require "coutil.system"
local stdin = system.stdin
local stdout = system.stdout
local runall = system.run

_G._G = _G
_G.coroutine = require "coroutine"
_G.debug = debug
_G.io = io
_G.math = math
_G.package = package
_G.os = os
_G.string = string
_G.table = table
_G.utf8 = require "utf8"

_G.memory = memory
_G.vararg = require "vararg"

_G.channel = require "coutil.channel"
_G.stateco = require "coutil.coroutine"
_G.event = require "coutil.event"
_G.mutex = require "coutil.mutex"
_G.promise = require "coutil.promise"
_G.queued = require "coutil.queued"
_G.spawn = spawn
_G.system = system
_G.threads = require "coutil.threads"

local function writeto(file, ...)
	for i = 1, select("#", ...) do
		file:write((select(i, ...)))
	end
end

local function traceerr(errmsg, level)
	return traceback(errmsg, level or 2)
end

local progname

local function report(message)
	if progname then
		writeto(stderr, progname, ": ");
	end
	writeto(stderr, message, "\n");
end

local function catcherr(errmsg)
	report(traceerr(errmsg, 3))
	return errmsg
end

function spawn.call(f, ...)
	return catch(catcherr, f, ...)
end

local buffer<const> = alloc(512)
local EOFMARK<const> = "<eof>"

local function readline()
	local result, errmsg = stdin:read(buffer)
	if not result then
		return false, errmsg
	end
	if buffer:get(result) == string.byte("\n") then
		result = result - 1
	end
	return buffer:tostring(1, result)
end

local function loadline()
	writeto(stdout, _PROMPT or "> ")
	local result, errmsg = readline()
	if not result then
		return false, errmsg
	end
	local line = result
	result, errmsg = load("return "..line, "=stdin")
	while not result do
		result, errmsg = load(line, "=stdin")
		if not result then
			if errmsg:sub(-#EOFMARK) ~= EOFMARK then
				return false, errmsg
			end
			writeto(stdout, _PROMPT2 or ">> ")
			result, errmsg = readline()
			if not result then
				return false, errmsg
			end
			line = line.."\n"..result
			result = false
		end
	end
	return result
end

local function handleresults(ok, ...)
	if not ok then
		report(...)
	elseif select("#", ...) > 0 then
		print(...)
	end
	return ok
end

local function doREPL()
	local oldprogname = progname
	progname = nil
	while true do
		local result, errmsg = loadline()
		if result then
			handleresults(xpcall(result, traceerr))
		elseif errmsg == "end of file" then
			break
		else
			report(errmsg)
		end
	end
	progname = oldprogname
end

local function dochunk(argv, result, errmsg)
	if result then
		return handleresults(xpcall(result, traceerr, unpack(argv or {})))
	end
	report(errmsg)
	return false
end

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
		report(message);
	end
	writeto(stderr, usage:format(progname));
end

local function printversion()
	writeto(stdout, _VERSION, "  Copyright (C) 1994-2024 Lua.org, PUC-Rio", "\n")
end

local pkgcfg = {}
for value in config:gmatch("([^\n]+)") do
	table.insert(pkgcfg, value)
end
local LUA_IGMARK<const> = pkgcfg[5]

spawn.call(function (...)

	if arg then
		for i = 0, -inf, -1 do
			if not arg[i] then
				break
			end
			progname = arg[i]
		end
	else
		script = getinfo(1, "S").source
		if script:sub(1, 1) == "@" then
			script = script:sub(2)
		end
		arg = { [0] = script, ... }
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
	end

	if flags.v then
		printversion()
	end

	if flags.E then
		getregistry().LUA_NOENV = true
	elseif not getregistry().LUA_NOENV then
		local init = getenv("COUTIL_INIT")
		if init then
			if init:sub(1, 1) == "@" then
				init = dochunk(nil, loadfile(init:sub(2)))
			else
				init = dochunk(nil, load(init, "="..name))
			end
			if not init then
				return
			end
		end
	end

	for index, value in ipairs(values) do
		if options[index] == 'W' then
			warn("@on")
		elseif options[index] == 'l' then
			local global, modname = value:match("^(.+)=(.+)$")
			if not global then
				global, modname = value, value
				local pos = global:find(LUA_IGMARK)
				if pos then
					global = global:sub(1, pos-1)
				end
			end
			_G[global] = require(modname)
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
		system.stderr:write(i)
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
				system.stderr:write(i)
			end
		end)))
		system.run()
	]])
end
pool:resize(3)

print "Hello, World!"
--]============================================================================]
