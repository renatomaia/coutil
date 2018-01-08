local _G = require "_G"                                                         --[[VERBOSE]] local verbose = require "coutil.verbose"
local assert = _G.assert
local type = _G.type

local os = require "os"
local gettime = os.time

local event = require "coutil.event"
local awaitevent = event.await

local timevt = require "coutil.time.event"
local setuptimer = timevt.create
local emituntil = timevt.emitall

local function waituntil(timestamp)
	setuptimer(timestamp)
	awaitevent(timestamp)
end

local module = {
	gettime = gettime,
	waituntil = waituntil,
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
		local nextwake = emituntil(gettime())                                       --[[VERBOSE]] verbose:time("next wake is ",nextwake)
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
	end
end

return module
