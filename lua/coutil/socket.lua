local _G = require "_G"                                                         --[[VERBOSE]] local verbose = require "coutil.verbose"
local ipairs = _G.ipairs
local next = _G.next
local pcall = _G.pcall
local setmetatable = _G.setmetatable
local tostring = _G.tostring
local type = _G.type

local math = require "math"
local inf = math.huge
local max = math.max
local min = math.min

local array = require "table"
local concat = array.concat
local unpack = array.unpack

local table = require "loop.table"
local copy = table.copy

local proto = require "loop.proto"
local clone = proto.clone

local oo = require "loop.base"
local class = oo.class

local Wrapper = require "loop.object.Wrapper"

local socketcore = require "socket.core"
local selectsockets = socketcore.select
local createtcp = socketcore.tcp
local createudp = socketcore.udp
local suspendprocess = socketcore.sleep
local gettime = socketcore.gettime

local event = require "coutil.event"
local awaitany = event.awaitany
local emitevent = event.emit

local sockevt = require "coutil.socket.event"
local watchsocket = sockevt.create
local forgetsocket = sockevt.cancel
local emitsockevents = sockevt.emitall

local timevt = require "coutil.time.event"
local setuptimer = timevt.create
local canceltimer = timevt.cancel

local time = require "coutil.time"
local setclock = time.setclock
local waketimers = time.run

setclock(gettime)

local function setupevents(socket, write, self)
	local deadline = self.deadline
	local timeout = self.timeout
	if timeout ~= nil then
		deadline = min(gettime() + timeout, deadline or inf)
	end
	if deadline == nil or deadline > gettime() then
		if deadline ~= nil then
			setuptimer(deadline)
		end
		return watchsocket(socket, write), deadline -- return events
	end
end

local function cancelevents(socket, write, deadline)
	if deadline ~= nil then
		canceltimer(deadline)
	end
	forgetsocket(socket, write)
end

local function idle(deadline)
	repeat
		local timeout = max(0, deadline - gettime())
		if not emitsockevents(timeout) then                                         --[[VERBOSE]] verbose:socket("suspend for ",timeout," seconds")
			suspendprocess(timeout)
		end
	until timeout == 0
end

local SockWrap = class{ __index = Wrapper.__index }

function SockWrap:__tostring()
	return tostring(self.__object)
end

local function wrap(ops, socket, ...)
	if type(socket) == "userdata" then
		socket:settimeout(0)
		socket = copy(ops, SockWrap{ __object = socket })
	end
	return socket, ...
end

local CoSock = {}

function CoSock:setdeadline(timestamp)
	local old = self.deadline                                                     --[[VERBOSE]] verbose:socket("deadline of ",self," set to ",timestamp, " (was ",old,")")
	self.deadline = timestamp
	return true, old
end

function CoSock:settimeout(timeout)
	local oldtm = self.timeout                                                    --[[VERBOSE]] verbose:socket("timeout of ",self," set to ",timeout, " (was ",oldtm,")")
	if not timeout or timeout < 0 then
		self.timeout = nil
	else
		self.timeout = timeout
	end
	return 1, oldtm
end

function CoSock:close()
	local socket = self.__object
	local result, errmsg = socket:close()                                         --[[VERBOSE]] verbose:socket("closing ",self, result and " successful" or " failed ("..tostring(errmsg)..")")
	emitevent(socket) -- wake threads reading the socket
	emitevent(self) -- wake threads writing the socket
	return result, errmsg
end

local CoTCP = copy(CoSock)

function CoTCP:connect(...)                                                     --[[VERBOSE]] verbose:socket(true, "connecting ",self," to ",...)
	-- connect the socket if possible
	local socket = self.__object
	local result, errmsg = socket:connect(...)
	
	-- check if the job has not yet been completed
	if not result and errmsg == "timeout" then
		local event, deadline = setupevents(socket, self, self)
		if event ~= nil then                                                        --[[VERBOSE]] verbose:socket(true, "waiting for connection establishment")
			-- wait for a connection completion and finish establishment
			awaitany(event, deadline)                                                 --[[VERBOSE]] verbose:socket(false, "waiting completed")
			-- cancel emission of events
			cancelevents(socket, self, deadline)
			-- try to connect again one last time before giving up
			result, errmsg = socket:connect(...)
			if not result and errmsg == "already connected" then
				result, errmsg = 1, nil -- connection was already established
			end
		end
	end                                                                           --[[VERBOSE]] verbose:socket(false, "connection ",result and "established" or "failed ("..tostring(errmsg)..")")
	return result, errmsg
end

function CoTCP:accept(...)                                                      --[[VERBOSE]] verbose:socket(true, "accepting a new connection")
	-- accept any connection request pending in the socket
	local socket = self.__object
	local result, errmsg = socket:accept(...)
	
	-- check if the job has not yet been completed
	if result then
		result = wrap(CoTCP, result)
	elseif errmsg == "timeout" then
		local event, deadline = setupevents(socket, nil, self)
		if event ~= nil then                                                        --[[VERBOSE]] verbose:socket(true, "waiting for new connection request")
			-- wait for a connection request signal
			awaitany(event, deadline)                                                  --[[VERBOSE]] verbose:socket(false, "waiting completed")
			-- cancel emission of events
			cancelevents(socket, nil, deadline)
			-- accept any connection request pending in the socket
			result, errmsg = socket:accept(...)
			if result then result = wrap(CoTCP, result) end
		end
	end                                                                           --[[VERBOSE]] verbose:socket(false, "new connection ",result and "accepted" or "failed ("..tostring(errmsg)..")")
	return result, errmsg
end

function CoTCP:send(data, i, j)                                                 --[[VERBOSE]] verbose:socket(true, "sending byte stream: ",verbose:escaped(data, i, j))
	-- fill space already avaliable in the socket
	local socket = self.__object
	local result, errmsg, lastbyte = socket:send(data, i, j)

	-- check if the job has not yet been completed
	if not result and errmsg == "timeout" then
		local event, deadline = setupevents(socket, self, self)
		if event ~= nil then                                                        --[[VERBOSE]] verbose:socket(true, "waiting for more space to write stream to be sent")
			-- wait for more space on the socket or a timeout
			while awaitany(event, deadline) == event do
				-- fill any space free on the socket one last time
				result, errmsg, lastbyte = socket:send(data, lastbyte+1, j)
				if result or errmsg ~= "timeout" then                                   --[[VERBOSE]] verbose:socket("stream was partially sent until byte ",lastbyte)
					break
				end
			end                                                                       --[[VERBOSE]] verbose:socket(false, "waiting completed")
			cancelevents(socket, self, deadline)
		end
	end                                                                           --[[VERBOSE]] verbose:socket(false, "stream sending ",result and "completed" or "failed ("..tostring(errmsg)..")")
	
	return result, errmsg, lastbyte
end

function CoTCP:receive(pattern, ...)                                            --[[VERBOSE]] verbose:socket(true, "receiving byte stream")
	-- get data already avaliable in the socket
	local socket = self.__object
	local result, errmsg, partial = socket:receive(pattern, ...)
	
	-- check if the job has not yet been completed
	if not result and errmsg == "timeout" then
		local event, deadline = setupevents(socket, nil, self)
		if event ~= nil then                                                        --[[VERBOSE]] verbose:socket(true, "waiting for new data to be read")
			-- initialize data read buffer with data already read
			local buffer = { partial }
			
			-- register socket for network event watch
			while awaitany(event, deadline) == event do -- otherwise it was a timeout
				-- reduce the number of required bytes
				if type(pattern) == "number" then
					pattern = pattern - #partial                                          --[[VERBOSE]] verbose:socket("got more ",#partial," bytes, waiting for more ",pattern)
				end
				-- read any data left on the socket one last time
				result, errmsg, partial = socket:receive(pattern)
				if result then
					buffer[#buffer+1] = result
					break
				else
					buffer[#buffer+1] = partial
					if errmsg ~= "timeout" then
						break
					end
				end
			end
		
			-- concat buffered data
			if result then
				result = concat(buffer)
			else
				partial = concat(buffer)
			end                                                                       --[[VERBOSE]] verbose:socket(false, "waiting completed")
		
			cancelevents(socket, nil, deadline)
		end
	end                                                                           --[[VERBOSE]] verbose:socket(false, "data reading ",result and "completed" or "failed ("..tostring(errmsg)..")")
	
	return result, errmsg, partial
end

local function getdatagram(opname, self, ...)                                   --[[VERBOSE]] verbose:socket(true, "receiving datagram")
	-- get data already avaliable in the socket
	local socket = self.__object
	local result, errmsg, port = socket[opname](socket, ...)
	-- check if the job has not yet been completed
	if not result and errmsg == "timeout" then
		local event, deadline = setupevents(socket, nil, self)
		if event ~= nil then                                                        --[[VERBOSE]] verbose:socket(true, "waiting for data")
			if awaitany(event, deadline) == event then
				result, errmsg, port = socket[opname](socket, ...)
			end                                                                       --[[VERBOSE]] verbose:socket(false, "waiting completed")
			cancelevents(socket, nil, deadline)
		end
	end                                                                           --[[VERBOSE]] verbose:socket(false, "data reading ",result and "completed" or "failed ("..tostring(errmsg)..")")
	
	return result, errmsg, port
end

local CoUDP = copy(CoSock)

for _, opname in ipairs{ "receive", "receivefrom" } do
	CoUDP[opname] = function (...)
		return getdatagram(opname, ...)
	end
end

--------------------------------------------------------------------------------
-- Wrapped Lua Socket API ------------------------------------------------------
--------------------------------------------------------------------------------

local module = clone(socketcore)

function module.tcp()
	return wrap(CoTCP, createtcp())
end

function module.udp()
	return wrap(CoUDP, createudp())
end

function module.select(recvt, sendt, timeout)                                   --[[VERBOSE]] verbose:socket(true, "selecting sockets ready")
	-- collect sockets so we don't rely on provided tables be left unchanged
	local wrapof = {}
	local recv, send
	if recvt and #recvt > 0 then
		recv = {}
		for index, wrap in ipairs(recvt) do
			local socket = wrap.__object
			wrapof[socket] = wrap
			recv[index] = socket
		end
	end
	if sendt and #sendt > 0 then
		send = {}
		for index, wrap in ipairs(sendt) do
			local socket = wrap.__object
			wrapof[socket] = wrap
			send[index] = socket
		end
	end
	
	-- if no socket is given then return
	if recv == nil and send == nil then
		return wrapof, wrapof, "timeout"
	end
	
	-- collect any ready socket
	local readok, writeok, errmsg = selectsockets(recv, send, 0)
	
	-- check if job has completed
	if
		timeout ~= 0 and
		errmsg == "timeout" and
		next(readok) == nil and
		next(writeok) == nil
	then                                                                          --[[VERBOSE]] verbose:socket(true, "waiting for sockets to become ready")
		-- setup events to wait
		local events = {}
		local deadline
		if timeout ~= nil and timeout > 0 then
			deadline = gettime() + timeout
			setuptimer(deadline)
			events[#events+1] = deadline
		end
		if recv ~= nil then
			for _, socket in ipairs(recv) do
				events[#events+1] = watchsocket(socket)
			end
		end
		if send ~= nil then
			for _, socket in ipairs(send) do
				events[#events+1] = watchsocket(socket, wrapof[socket])
			end
		end
		-- block until some socket event is signal or timeout
		if awaitany(unpack(events)) ~= deadline then
			-- collect all ready sockets
			readok, writeok, errmsg = selectsockets(recv, send, 0)
		end                                                                         --[[VERBOSE]] verbose:socket(false, "waiting completed")
		-- unregister events to wait
		if recv ~= nil then
			for _, socket in ipairs(recv) do
				forgetsocket(socket)
			end
		end
		if send ~= nil then
			for _, socket in ipairs(send) do
				forgetsocket(socket, wrapof[socket])
			end
		end
	end
	
	-- replace sockets for the corresponding cosocket wrap
	for index, socket in ipairs(readok) do
		local wrap = wrapof[socket]
		readok[index] = wrap
		readok[wrap] = true
		readok[socket] = nil
	end
	for index, socket in ipairs(writeok) do
		local wrap = wrapof[socket]
		writeok[index] = wrap
		writeok[wrap] = true
		writeok[socket] = nil
	end                                                                           --[[VERBOSE]] verbose:socket(false, "returning sockets ready")
	
	return readok, writeok, errmsg
end

function module.run(timeout)
	repeat                                                                        --[[VERBOSE]] verbose:socket(true, "running timer emitter")
		local nextwake = waketimers(idle, timeout)                                  --[[VERBOSE]] verbose:socket(false, "timers emitted, next wake is ",nextwake)
		if nextwake ~= nil then return nextwake end
		if not emitsockevents(timeout and max(0, timeout - gettime())) then         --[[VERBOSE]] verbose:socket("no socket nor time event pending")
			break
		end
	until timeout ~= nil and timeout >= gettime()
end

return module
