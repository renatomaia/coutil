local memory = require "memory"
local alloc = memory.create

local spawn = require "coutil.spawn"
local catch = spawn.catch

local system = require "coutil.system"
local stderr = system.stderr
local stdin = system.stdin
local stdout = system.stdout

local function writeto(file, ...)
	for i = 1, select("#", ...) do
		file:write((select(i, ...)))
	end
end

local function catcherr(errmsg)
	io.stderr:write(debug.traceback(errmsg), "\n")
	io.stderr:flush()
	return errmsg
end

local function spawn(f, ...)
	return catch(catcherr, f, ...)
end

local buffsz = 256
local buffer = alloc(buffsz)
local filled = {}
local start, finish = 1, 0

spawn(function ()
	writeto(stdout, "> ")
	while true do
		local result, errmsg = stdin:read(buffer, finish+1)
		if not result then
			writeto(stderr, errmsg, "\n")
			break
		end
		local newfinish = finish+result
		local breakline = buffer:find("\n", finish+1, newfinish)
		if breakline then
			if #filled > 0 then
				result = alloc(breakline-start+buffsz*#filled)
				local index = 1
				for i = 1, #filled do
					result:fill(filled[i], index, index-1+buffsz)
					index = index+buffsz
					filled[i] = nil
				end
				result:fill(buffer, index, index-1+breakline-start)
				result = result:tostring()
			else
				result = buffer:tostring(start, breakline-1)
			end
			if breakline == newfinish then
				start, newfinish = 1, 0
			else
				start = breakline+1
			end
			result, errmsg = load(result)
			if result then
				result, errmsg = xpcall(result, debug.traceback)
				if result then
					writeto(stdout, tostring(errmsg), "\n")
				else
					writeto(stderr, errmsg, "\n")
				end
			else
				writeto(stderr, errmsg, "\n")
			end
			writeto(stdout, "> ")
		end
		finish = newfinish
		if finish == buffsz then
			if start == 1 then
				table.insert(filled, buffer)
				buffer = alloc(buffsz)
				finish = 0
			else
				finish = 1+finish-start
				buffer:fill(buffer, 1, finish, start)
				start = 1
			end
		end
	end
end)

_G.spawn = spawn
_G.system = system

system.run()

--[============================================================================[
spawn(function ()
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
