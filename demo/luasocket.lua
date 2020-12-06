local _G = require "_G"
local assert = _G.assert
local error = _G.error
local getmetatable = _G.getmetatable
local ipairs = _G.ipairs
local pcall = _G.pcall
local tonumber = _G.tonumber
local tostring = _G.tostring
local type = _G.type

local coroutine = require "coroutine"
local newcoro = coroutine.create
local resume = coroutine.resume
local running = coroutine.running
local status = coroutine.status
local yield = coroutine.yield

local math = require "math"
local max = math.max
local min = math.min

local string = require "string"
local strfind = string.find
local strmatch = string.match
local strsub = string.sub

local debug = require "debug"
local setmetatable = debug.setmetatable

local vararg = require "vararg"
local varange = vararg.range

local memory = require "memory"
local memalloc = memory.create
local memfill = memory.fill
local memfind = memory.find
local memget = memory.get
local memrealloc = memory.resize
local mem2str = memory.tostring

local system = require "coutil.system"
local now = system.time
local sleep = system.suspend
local newaddr = system.address
local newsocket = system.socket
local resolveaddr = system.findaddr

local defaultsize = 8192

local function checkclass(self, expected)
	if self.class ~= expected then
		local actual = self.class or type(self)
		error("bad argument #1 ("..expected.." expected, but got "..actual..")")
	end
end

local toaddr do
	local addrset = getmetatable(newaddr("ipv4")).__newindex

	function toaddr(addr, host, port)
		if not pcall(addrset, addr, "literal", host) then
			local found, errmsg = resolveaddr(host, port, "4")
			if not found then return nil, errmsg end
			found:getaddress(addr)
		else
			addr.port = port
		end
		return addr
	end
end

local function wrapsock(class, metatable, domain, result, errmsg)
	if not result then return nil, errmsg end
	return setmetatable({
		socket = result,
		address = newaddr(domain),
		buffer = memalloc(defaultsize),
		class = class,
	}, metatable)
end

local function timerbody(self)
	while true do
		while self.thread do
			if system.suspend(self.timeout) then
				assert(resume(self.thread, nil, timerbody))
			end
		end
		self = yield()
	end
end

local function starttimer(self)
	local timeout = self.timeout
	if timeout == nil then
		assert(self.thread == nil)
	else
		self.thread = running()
		if status(self.timer) == "suspended" then
			assert(resume(self.timer, self))
		end
	end
end

local function canceltimer(self, ...)
	if self.thread == running() then
		self.thread = nil
		local result, errmsg = ...
		if not result and errmsg == timerbody then
			return nil, "timeout"
		end
		assert(resume(self.timer, nil, "cancel"))
	end
	return ...
end

local function connectsock(self, host, port, class)
	local socket = self.socket
	if socket == nil then return nil, "closed" end
	local addr, errmsg = toaddr(self.address, host, port)
	if not addr then return nil, errmsg end
	starttimer(self)
	local ok, errmsg = canceltimer(self, socket:connect(addr))
	if not ok then return nil, errmsg end
	self.class = class
	return true
end

local tcp = {
	class = "tcp{master}",
	sendmaxsz = defaultsize,
	first = 1,
	last = 0,
}
tcp.__index = tcp

do
	function tcp:settimeout(value, mode)
		assert(mode == nil or (#mode == 1 and strfind("btr", mode, 1 , true)),
			"invalid mode")
		if value ~= nil then
			assert(type(value) == "number", "number expected")
			if value < 0 then value = nil end
		end
		self.timeout = value
		if value ~= nil and self.timer == nil then
			self.timer = newcoro(timerbody)
		end
	end
end

do
	local function rawtostring(value)
		local mt = getmetatable(value)
		local tostr = mt.__tostring
		if tostr ~= nil then
			mt.__tostring = nil
		end
		local str = tostring(value)
		if tostr ~= nil then
			mt.__tostring = tostr
		end
		return strmatch(str, "%w+: (0x%x+)$")
	end

	function tcp:__tostring()
		return self.class..": "..rawtostring(self)
	end
end

function tcp:accept()
	checkclass(self, "tcp{server}")
	local socket = self.socket
	if socket == nil then return nil, "closed" end
	starttimer(self)
	return wrapsock("tcp{client}", tcp, self.address.type,
		canceltimer(self, socket:accept()))
end

function tcp:bind(host, port)
	local res, errmsg = self.socket:bind(toaddr(self.address, host, port))
	if not res then return nil, errmsg end
	-- force actual binding
	res, errmsg = self.socket:getaddress("self", self.address)
	if not res then return nil, errmsg end
	return 1
end

function tcp:close()
	local socket = self.socket
	if socket == nil then return nil, "closed" end
	self.timeout = nil
	self.timer = nil
	self.socket = nil
	return socket:close()
end

function tcp:connect(host, port)
	checkclass(self, "tcp{master}")
	return connectsock(self, host, port, "tcp{client}")
end

function tcp:dirty()
	return self.first <= self.last
end

function tcp:getfd()
	local socket = self.socket
	if socket == nil then return -1 end
	return 0
end

do
	local function getaddr(self, side)
		local addr = self.address
		local res, errmsg = self.socket:getaddress(side, addr)
		if not res then return nil, errmsg end
		return addr.literal, addr.port
	end

	function tcp:getpeername()
		return getaddr(self, "peer")
	end

	function tcp:getsockname()
		return getaddr(self, "self")
	end
end

function tcp:getstats()
	checkclass(self, "tcp{client}")
	error("not supported")
end

do
	local lstmt do
		local lstsck = assert(newsocket("passive", "ipv4"))
		lstmt = getmetatable(lstsck)
		lstsck:close()
	end

	function tcp:listen(backlog)
		checkclass(self, "tcp{master}")
		local socket = self.socket
		if socket == nil then return nil, "closed" end
		local oldmt = getmetatable(socket)
		setmetatable(socket, lstmt)
		local result, errmsg = socket:listen(backlog or 32)
		if not result then
			setmetatable(socket, oldmt)
			return nil, errmsg
		end
		self.class = "tcp{server}"
		return true
	end
end

do
	local pat2term = { ["*l"] = "\n" }

	local function initbuf(self, prefix, required)
		local buffer = self.buffer
		local first = self.first
		local last = self.last
		local datasz = 1+last-first
		local pfxsz = #prefix
		local pfxidx = first-pfxsz
		local bufsz = #buffer
		local newbuf
		local reqsz = required == nil and 1 or max(0, required-datasz)
		if pfxsz+datasz+reqsz <= bufsz then
			newbuf = buffer
		end
		if newbuf == buffer and pfxidx > 0 and first+datasz+reqsz-1 <= bufsz then
			if pfxsz > 0 then memfill(buffer, prefix, pfxidx, first-1) end
		else
			local dataidx = first
			pfxidx, first, last = 1, pfxsz+1, pfxsz+datasz
			if newbuf == nil then
				bufsz = last+max(reqsz, bufsz)
				newbuf = memalloc()
				memrealloc(newbuf, bufsz)
			end
			if datasz > 0 then memfill(newbuf, buffer, first, last, dataidx) end
			if pfxsz > 0 then memfill(newbuf, prefix, 1, pfxsz) end
		end
		return newbuf, pfxidx, first, last
	end

	local function incbuf(self, buffer)
		local defbuf = self.buffer
		local bufsz = #buffer
		local newsz = bufsz+#defbuf
		if buffer == defbuf then
			buffer = memalloc()
			memrealloc(buffer, newsz)
			memfill(buffer, defbuf, 1, bufsz)
		else
			memrealloc(buffer, newsz)
		end
		return buffer
	end

	function tcp:receive(pattern, prefix)
		checkclass(self, "tcp{client}")
		local socket = self.socket
		if socket == nil then return nil, "closed" end
		assert(not self.receiving, "in use")

		if pattern == nil then pattern = "*l" end
		if prefix == nil then prefix = "" end
		local result, errmsg, partial = 0 -- to signal success
		local pattype = type(pattern)
		local required = (pattype == "number") and pattern or nil
		local buffer, pfxidx, first, last = initbuf(self, prefix, required)
		self.receiving = true
		starttimer(self)
		local reqidx
		if pattype == "number" then
			reqidx = pfxidx+pattern-1
			while reqidx > last do
				result, errmsg = socket:receive(buffer, last+1, reqidx)
				if not result then
					reqidx = last
					break
				end
				last = last+result
			end
			partial = mem2str(buffer, pfxidx, reqidx)
		else
			local term = pat2term[pattern]
			assert(term ~= nil or pattern == "*a", "invalid pattern")
			while true do
				reqidx = term and memfind(buffer, term, first, last)
				if reqidx ~= nil then
					break
				else
					first = last+1
					if first > #buffer then buffer = incbuf(self, buffer) end
					result, errmsg = socket:receive(buffer, first)
					if not result then
						reqidx = last
						break
					end
					last = last+result
				end
			end
			local endidx = reqidx
			if result then
				if term == "\n" and memget(buffer, reqidx-1) == 13 then
					endidx = reqidx-2
				elseif term ~= nil then
					endidx = reqidx-#term
				end
			end
			partial = mem2str(buffer, pfxidx, endidx)
			if pattern == "*a" and errmsg == "end of file" and #partial > 0 then
				result = 0 -- to signal success
			end
		end
		first = reqidx+1
		result, errmsg = canceltimer(self, result, errmsg)
		self.receiving = false
		if first > last then
			self.first, self.last = nil, nil
		elseif buffer == self.buffer then
			self.first, self.last = first, last
		else
			local datasz = 1+last-first
			memfill(self.buffer, buffer, 1, datasz, first)
			self.first, self.last = 1, datasz
		end

		if not result then
			if errmsg == "end of file" then
				errmsg = "closed"
			end
			return nil, errmsg, partial
		end
		return partial
	end
end

function tcp:send(data, first, last)
	checkclass(self, "tcp{client}")
	local socket = self.socket
	if socket == nil then return nil, "closed" end
	assert(not self.sending, "in use")

	if first == nil then first = 1 end
	if last == nil then last = #data end
	local sendmaxsz = self.sendmaxsz

	if first > last then
		return 0, nil, nil
	end
	self.sending = true
	starttimer(self)
	local result, errmsg = socket:send(data, first, last)
	if not result then
		result = 1+last-first
	end
	result, errmsg = canceltimer(self, result, errmsg)
	self.sending = false
	if not result then
		return nil, errmsg, result
	end
	return result, nil, nil
end

function tcp:setfd(fd)
	error("not supported")
end

do
	local name2opt = {
		keepalive = "keepalive",
		["tcp-nodelay"] = "nodelay",
		reuseaddr = false,
	}

	function tcp:setoption(name, value)
		local option = name2opt[name]
		assert(option ~= nil, "unsupported option")
		if not option then return 1 end
		return self.socket:setoption(option, value)
	end
end

tcp.setpeername = tcp.connect
tcp.setsockname = tcp.bind

function tcp:setstats()
	checkclass(self, "tcp{client}")
	error("not supported")
end

function tcp:shutdown(mode)
	checkclass(self, "tcp{client}")
	return self.socket:shutdown(mode)
end

local udp = {
	class = "udp{unconnected}",
	last = 0,
	close = tcp.close,
	getfd = tcp.getfd,
	getpeername = tcp.getpeername,
	getsockname = tcp.getsockname,
	setfd = tcp.setfd,
	setoption = tcp.setoption,
	setsockname = tcp.bind,
	settimeout = tcp.settimeout,
}
udp.__index = udp

function udp:dirty()
	return self.last > 0
end

do
	local function recvdgram(self, size, ...)
		local socket = self.socket
		if socket == nil then return nil, "closed" end
		assert(not self.receiving, "in use")

		local result, errmsg = self.last
		if result == 0 then
			local buffer = self.buffer

			self.receiving = true
			starttimer(self)
			repeat
				result, errmsg = socket:receive(buffer, 1, size, ...)
				if not result then break end
			until result == size or result > 0
			result, errmsg = canceltimer(self, result, errmsg)
			self.receiving = false

			if not result then return nil, errmsg end
		else
			self.last = 0
		end

		return mem2str(buffer, 1, result)
	end

	function udp:receive(size)
		checkclass(self, "udp{connected}")
		return recvdgram(self, size)
	end

	function udp:receivefrom(size)
		checkclass(self, "udp{unconnected}")
		local addr = self.address
		local result, errmsg = recvdgram(self, size, nil, addr)
		if not result then return nil, errmsg end
		return result, addr.literal, addr.port
	end
end

function udp:send(data)
	checkclass(self, "udp{connected}")
	return self.socket:send(data)
end

function udp:sendto(data, host, port)
	checkclass(self, "udp{unconnected}")
	return self.socket:send(data, 1, -1, toaddr(self.address, host, port))
end

function udp:setpeername(host, port)
	return connectsock(self, host, port, host == "*" and "udp{unconnected}"
	                                                  or "udp{connected}")
end

local dns = {}

function dns.gethostname()
	error("not supported")
end

function dns.tohostname()
	error("not supported")
end

function dns.toip()
	error("not supported")
end

local socket = {
	_VERSION = "LuaSocket 2.0.2",
	dns = dns,
	gettime = now,
	sleep = sleep,
}

function socket.newtry(func)
	return function (ok, ...)
		if not ok then
			pcall(func)
			error{(...)}
		end
		return ...
	end
end

do
	local function cont(ok, ...)
		if not ok then
			local err = ...
			if type(err) == "table" then
				return nil, err[1]
			end
			error(err)
		end
		return ...
	end
	function socket.protect(func)
		return function (...)
			return cont(pcall(func, ...))
		end
	end
end

do
	function socket.tcp()
		return wrapsock(nil, tcp, "ipv4", newsocket("stream", "ipv4"))
	end

	function socket.udp()
		return wrapsock(nil, udp, "ipv4", newsocket("datagram", "ipv4"))
	end

	local sockcls = { [tcp]=true, [udp]=true }

	local function addresult(list, sock)
		list[#list+1] = sock
		list[sock] = true
	end

	local function collectpending(pending, list, result, event)
		if list ~= nil then
			for _, value in ipairs(list) do
				if sockcls[getmetatable(value)] ~= nil and value.socket ~= nil then
					local sock = value.socket
					if event == "w" or sock:dirty() then
						addresult(result, value)
					else
						pending[value] = true
					end
				end
			end
		end
		return next(pending) ~= nil
	end

	local function wait2read(self, thread)
		self.last = self.socket:receive(self.buffer)
		if self.last then
			resume(thread, wait2read, self)
		end
	end

	function socket.select(recvt, sendt, timeout)
		local recvok, sendok, result, errmsg = {}, {}
		local pending = {}
		if collectpending(pending, recvt, recvok, "r") or
		   collectpending(pending, sendt, sendok, "w") then
			local thread = running()
			for socket in pairs(pending) do
				local waiter = newcoro(wait2read)
				resume(waiter, socket, thread)
				pending[socket] = waiter
			end
			if timeout == nil or timeout < 0 then
				result, errmsg = yield()
			else
				result, errmsg = sleep(timeout)
			end
			if result == wait2read then
				local ready = errmsg
				pending[ready] = nil
				addresult(recvok, ready)
			end
			for socket, waiter in pairs(pending) do
				resume(waiter, nil, "cancel")
			end
		else
			result, errmsg = sleep(timeout)
			if result then
				errmsg = "timeout"
			end
		end
		return recvok, sendok, errmsg
	end
end

function socket.skip(c, ...)
	varange(1, -c, ...)
end

function socket.__unload()
	-- empty
end

if LUASOCKET_DEBUG then
	local function wrap(func)
		return function (...)
			local start = now()
			local result, errmsg, partial = func(...)
			return result, errmsg, partial, now() - start
		end
	end
	tcp.receive = wrap(tcp.receive)
	tcp.send = wrap(tcp.send)
	socket._DEBUG = true
end

return socket
