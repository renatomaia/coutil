local _G = require "_G"                                                         --[[VERBOSE]] local verbose = require "coutil.verbose"
local ipairs = _G.ipairs

local ArrayedSet = require "loop.collection.ArrayedSet"

local event = require "coutil.event"
local emitevent = event.emitall
local pendingevent = event.pending

local socketcore = require "socket.core"
local selectsockets = socketcore.select

local reading = ArrayedSet()
local writing = ArrayedSet()
local writeof = {}

local module = {}

function module.create(socket, write)
	local set, event = reading, socket
	if write ~= nil then
		writeof[socket] = write
		set, event = writing, write
	end
	set:add(socket)
	return event
end

function module.cancel(socket, write)
	local set, event = reading, socket
	if write ~= nil then
		set, event = writing, write
	end
	if not pendingevent(event) and set:remove(socket) then
		if event == write then
			writeof[socket] = nil
		end
		return true
	end
	return false
end

function module.emitall(timeout)
	if #reading > 0 or #writing > 0 then                                          --[[VERBOSE]] verbose:socket(true, "wait socket event for ",timeout," seconds")
		local recvok, sendok = selectsockets(reading, writing, timeout)
		for _, socket in ipairs(recvok) do                                          --[[VERBOSE]] verbose:socket("emit read ready for ",socket)
			emitevent(socket)
		end
		for _, socket in ipairs(sendok) do                                          --[[VERBOSE]] verbose:socket("emit write ready for ",socket)
			emitevent(writeof[socket])
		end                                                                         --[[VERBOSE]] verbose:socket(false, "socket events emitted")
		return true
	end
	return false
end

return module
