local _G = require "_G"
local assert = _G.assert
local error = _G.error
local getmetatable = _G.getmetatable
local ipairs = _G.ipairs
local pcall = _G.pcall
local select = _G.select
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
local nameaddr = system.nameaddr
local resolveaddr = system.findaddr
local procinfo = system.procinfo

local defaultsize = 8192

local function checkclass(self, expected)
	if self.class ~= expected then
		local actual = self.class or type(self)
		error("bad argument #1 ("..expected.." expected, but got "..actual..")")
	end
end

local addrset = getmetatable(newaddr("ipv4")).__newindex

local function toaddr(addr, host, port)
	if not pcall(addrset, addr, "literal", host) then
		local found<close>, errmsg = resolveaddr(host, port, "4")
		if not found then return nil, errmsg end
		found:getaddress(addr)
	else
		addr.port = port
	end
	return addr
end

local function wrapsock(class, metatable, domain, result, errmsg)
	if not result then return nil, errmsg end
	return setmetatable({
		socket = result,
		address = newaddr(domain),
		buffer = memalloc(defaultsize),
		readtimer = {},
		writetimer = {},
		class = class,
	}, metatable)
end

local function timerbody(self)
	while true do
		while self.thread do
			if sleep(self.timeout) then
				assert(resume(self.thread, nil, timerbody))
			end
		end
		self = yield()
	end
end

local function starttimer(self, options)
	local activemode = self.active
	if not activemode then
		activemode = options.mode or "none"
		if activemode ~= "none" then
			self.thread = running()
			if status(self.timer) == "suspended" then
				assert(resume(self.timer, self))
			end
		end
		self.active = activemode
	elseif activemode == "block" then
		assert(resume(self.timer, false, "reset"))
	end
end

local function canceltimer(self, ...)
	local activemode = self.active or "none"
	self.active = false
	if activemode ~= "none" then
		self.thread = false
		local result, errmsg = ...
		if not result and errmsg == timerbody then
			return nil, "timeout"
		end
		assert(resume(self.timer, false, "cancel"))
	end
	return ...
end

local function connectsock(self, host, port, class)
	local socket = self.socket
	if not socket then return nil, "closed" end
	local addr, errmsg = toaddr(self.address, host, port)
	if not addr then return nil, errmsg end
	local timer = self.writetimer
	if timer.active then return nil, "in use" end
	starttimer(timer, self)
	local ok, errmsg = canceltimer(timer, socket:connect(addr))
	if not ok then return nil, errmsg end
	self.class = class
	return true
end

local tcp = {
	class = "tcp{master}",
	first = 1,
	last = 0,
}
tcp.__index = tcp

do
	local modes = {
		b = "block",
		t = "total",
		r = "total",
	}
	local timers = { "readtimer", "writetimer" }
	function tcp:settimeout(value, mode)
		if value then
			assert(type(value) == "number", "number expected")
			if value < 0 then value = false end
		end
		if value then
			self.mode = assert(modes[strsub(mode or "b", 1, 1)], "invalid mode")
			for _, field in ipairs(timers) do
				local optimer = self[field]
				optimer.timeout = value
				if not optimer.timer then
					optimer.timer = newcoro(timerbody)
				end
			end
		else
			self.mode = false
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
	if not socket then return nil, "closed" end
	local timer = self.readtimer
	if timer.active then return nil, "in use" end
	starttimer(timer, self)
	return wrapsock("tcp{client}", tcp, self.address.type,
		canceltimer(timer, socket:accept()))
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
	if not socket then return nil, "closed" end
	self.timeout = false
	self.timer = false
	self.socket = false
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
	if not socket then return -1 end
	return math.huge
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
		if not socket then return nil, "closed" end
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
		local reqsz = required and max(0, required-datasz) or 1
		if pfxsz+datasz+reqsz <= bufsz then
			newbuf = buffer
		end
		if newbuf == buffer and pfxidx > 0 and first+datasz+reqsz-1 <= bufsz then
			if pfxsz > 0 then memfill(buffer, prefix, pfxidx, first-1) end
		else
			local dataidx = first
			pfxidx, first, last = 1, pfxsz+1, pfxsz+datasz
			if not newbuf then
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
		if not socket then return nil, "closed" end
		local timer = self.readtimer
		if timer.active then return nil, "in use" end

		if not pattern then pattern = "*l" end
		if not prefix then prefix = "" end
		local result, errmsg, partial = 0 -- to signal success
		local pattype = type(pattern)
		local required = (pattype == "number") and pattern or false
		local buffer, pfxidx, first, last = initbuf(self, prefix, required)
		local reqidx
		if pattype == "number" then
			reqidx = pfxidx+pattern-1
			while reqidx > last do
				starttimer(timer, self)
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
			assert(term or pattern == "*a", "invalid pattern")
			while true do
				reqidx = term and memfind(buffer, term, first, last)
				if reqidx then
					break
				else
					first = last+1
					if first > #buffer then buffer = incbuf(self, buffer) end
					starttimer(timer, self)
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
				elseif term then
					endidx = reqidx-#term
				end
			end
			partial = mem2str(buffer, pfxidx, endidx)
			if pattern == "*a" and errmsg == "end of file" and #partial > 0 then
				result = 0 -- to signal success
			end
		end
		first = reqidx+1
		result, errmsg = canceltimer(timer, result, errmsg)
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
	if not socket then return nil, "closed" end
	local timer = self.writetimer
	if timer.active then return nil, "in use" end

	if not first then first = 1 end
	if not last then last = #data end

	if first > last then
		return 0, nil, nil
	end
	starttimer(timer, self)
	local result, errmsg = canceltimer(timer, socket:send(data, first, last))
	if not result then
		if errmsg == "broken pipe" then errmsg = "closed" end
		return nil, errmsg, 1+last-first
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
		reuseaddr = true,
	}

	function tcp:setoption(name, value)
		local option = name2opt[name]
		assert(option, "unsupported option")
		if option == true then return 1 end
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
		if not socket then return nil, "closed" end
		local timer = self.readtimer
		if timer.active then return nil, "in use" end

		local result, errmsg = self.last
		if result == 0 then
			local buffer = self.buffer

			repeat
				starttimer(timer, self)
				result, errmsg = socket:receive(buffer, 1, size, ...)
				if not result then break end
			until result == size or result > 0
			result, errmsg = canceltimer(timer, result, errmsg)

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

local dns = {} do
	local function resolve(address)
		local addr = newaddr("ipv4")
		local result<const>, errmsg = {
			name = nil,
			alias = {},
			ip = {},
		}

		-- pick a name
		if pcall(addrset, addr, "literal", address) then
			table.insert(result.ip, address)
			address, errmsg = nameaddr(addr)
			if not address then return nil, errmsg end
		else
			result.name = address
		end

		-- get canonical name
		result.name, errmsg = nameaddr(address)
		if not result.name then return nil, errmsg end
		if result.name ~= address then
			table.insert(result.alias, address)
		end

		-- get IP addresses
		local found<close>, errmsg = resolveaddr(result.name, 0, "4")
		if not found then return nil, errmsg end
		repeat
			table.insert(result.ip, found:getaddress(addr).literal)
		until not found:next()

		return result
	end

	function dns.toip(...)
		local result, errmsg = resolve(...)
		if not result then return nil, errmsg end
		return result.ip[1], result
	end

	function dns.tohostname(...)
		local result, errmsg = resolve(...)
		if not result then return nil, errmsg end
		return result.name, result
	end

	function dns.gethostname()
		return procinfo("n")
	end
end

local socket = {
	_VERSION = "LuaSocket 2.0.2",
	dns = dns,
	sleep = sleep,
	skip = select,
}

function socket.gettime()
	return now("epoch")
end

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
		if list then
			for _, value in ipairs(list) do
				if sockcls[getmetatable(value)] and value.socket then
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

	local function suspend(timeout)
		if not timeout or timeout < 0 then
			return yield()
		end
		return sleep(timeout)
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
			result, errmsg = suspend(timeout)
			if result == wait2read then
				local ready = errmsg
				pending[ready] = nil
				addresult(recvok, ready)
			elseif result then
				errmsg = "timeout"
			else
				errmsg = "coroutine resumed"
			end
			for socket, waiter in pairs(pending) do
				resume(waiter, nil, "cancel")
			end
		else
			result, errmsg = suspend(timeout)
			if result then
				errmsg = "timeout"
			else
				errmsg = "coroutine resumed"
			end
		end
		return recvok, sendok, errmsg
	end
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
