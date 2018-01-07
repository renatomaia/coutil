local _G = require "_G"                                                         --[[VERBOSE]] local verbose = require "coutil.verbose"
local assert = _G.assert
local type = _G.type

local array = require "table"
local insert = array.insert
local remove = array.remove

local math = require "math"
local floor = math.floor
local inf = math.huge

local os = require "os"
local gettime = os.time

local event = require "coutil.event"
local awaitevent = event.await
local emitevent = event.emit
local pendingevent = event.pending

local function sortedinsert(list, value, i, j)
	if i > j then
		return insert(list, i, value)
	else
		local m = floor((i+j)/2)
		local pivot = list[m]
		if value < pivot then
			return sortedinsert(list, value, i, m-1)
		elseif value > pivot then
			return sortedinsert(list, value, m+1, j)
		end
	end
end

local function sortedremove(list, value, i, j)
	if i <= j then
		local m = floor((i+j)/2)
		local pivot = list[m]
		if value < pivot then
			return sortedremove(list, value, i, m-1)
		elseif value > pivot then
			return sortedremove(list, value, m+1, j)
		else
			return remove(list, m)
		end
	end
end

local waketimes = {}

local function emituntil(timestamp)                                             --[[VERBOSE]] verbose:time(true, "emitting timers before ",timestamp)
	while waketimes[1] ~= nil and waketimes[1] <= timestamp do                    --[[VERBOSE]] verbose:time("emit timer ",waketimes[1])
		emitevent(remove(waketimes, 1))
	end                                                                           --[[VERBOSE]] verbose:time(false, "timers emitted")
end

local function setuptimer(timestamp)
	assert(type(timestamp) == "number", "bad argument #1 (number expected)")
	if timestamp ~= inf then                                                      --[[VERBOSE]] verbose:time("setup timer ",timestamp)
		sortedinsert(waketimes, timestamp, 1, #waketimes)
	end
end

local function canceltimer(timestamp)
	assert(type(timestamp) == "number", "bad argument #1 (number expected)")
	if not pendingevent(timestamp) then                                           --[[VERBOSE]] verbose:time("clear timer ",timestamp)
		sortedremove(waketimes, timestamp, 1, #waketimes)
	end
end

local function waituntil(timestamp)
	setuptimer(timestamp)
	awaitevent(timestamp)
end

local module = {
	setuptimer = setuptimer,
	canceltimer = canceltimer,
	waituntil = waituntil,
	gettime = gettime,
}

function module.setclock(func)
	assert(type(func) == "function", "bad argument #1 (function expected)")
	gettime = func
	module.gettime = func
end

function module.sleep(delay)
	waituntil(gettime()+delay)
end

function module.run(idle, timeout)
	while true do
		local nextwake = waketimes[1]                                               --[[VERBOSE]] verbose:time("next wake is ",nextwake)
		if nextwake == nil then
			return
		end
		local now = gettime()
		if timeout ~= nil and timeout <= now then                                   --[[VERBOSE]] verbose:time("time run reached timeout")
			return nextwake
		end
		if nextwake > now then                                                      --[[VERBOSE]] verbose:time(true, "wait until next wake")
			idle(nextwake)                                                            --[[VERBOSE]] verbose:time(false, "wait done")
		end
		emituntil(gettime())
	end
end

return module
