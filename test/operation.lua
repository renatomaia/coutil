local memory = require "memory"
local system = require "coutil.system"
local channel = require "coutil.channel"
local stateco = require "coutil.coroutine"

local case, done, casesuffix = case, done do
	local currentop

	local backup = case
	function case(name, op)
		if casesuffix ~= nil then
			name = name.." "..casesuffix
		end
		backup(name)
		op:setup()
		currentop = op
	end

	local backup = done
	function done()
		currentop:teardown()
		backup()
	end
end

local oncallbacks do
	local ch = channel.create("channel used to run in UV pool I/O phase")
	function oncallbacks(f, ...)
		spawn(function (...)
			system.awaitch(ch)
			return f(...)
		end, ...)
		channel.sync(ch)
	end
end

local sco = stateco.load([[
local coroutine = require "coroutine"
repeat until coroutine.yield()
]])

local addr = {
	system.address("ipv4", "127.0.0.1:65432"),
	system.address("ipv4", "127.0.0.1:65433"),
}

-- Thread Operation Tests

local function testOpAndYield(op) -- O2,C5,Y2,F1
                                  -- O1,C1,Y1
	case("scheduled yield", op)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O1|O2
		a = true
		coroutine.yield() -- Y1|Y2
		a = false
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	gc()
	assert(system.run() == false) -- (C1,Y1|C5,Y2,F1)
	assert(a == true)

	done()
end

local function testOpTwice(op) -- O2,C5,(O6,C5,)+Y2,F1
                               -- O1,C1,(O3,C1,)+Y1
	case("reschedule", op)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		op:await(2) -- O3|O6
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		op:trigger(2) -- ...C1|C5
	end)

	gc()
	assert(system.run() == false) -- (C1,O3,C1,Y1|C5,06,C5,Y2,F1)
	assert(a == true)

	done()
end

local function testOpBeforeCancelOp(op) -- O2,C5,Y2,(O10,R6,)+F1
 	case("yield and cancel repeat", op)

	local a, co
	spawn(function ()
		co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		op:await(2, "fail") -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(co)) -- O10
		assert(a == 2)
		assert(coroutine.resume(co, nil, "canceled")) -- R6
		assert(a == true)
		op:trigger(2) -- ...C5
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,R6,F1
	assert(a == true)
	co = nil

	done()
end

local function testOpBeforeReqOp(op) -- O2,C5,Y2,O10,F2,C1,Y1
	case("yield and call request op.", op)

	local a, co
	spawn(function ()
		co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		system.resume(sco) -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(co)) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,F2,C1,Y1
	assert(a == true)
	co = nil

	done()
end

local function testOpBeforeThrOp(op) -- O2,C5,Y2,O10,F3,C5,Y2,F1
	case("yield and call thread op.", op)

	local a, co
	spawn(function ()
		co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		assert(system.suspend()) -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(co)) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,F3,C5,Y2,F1
	assert(a == true)
	co = nil

	done()
end

local function testOpAndCancelOp(op) -- O2,C5,O7,R6,F1
                                     -- O1,C1,O4,R4,F1
	case("and cancel thread op.", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		local ok, res = system.suspend(10) -- O4|O7
		assert(not ok)
		assert(res == "canceled")
		a = true
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R4|R6
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- (C1,O4,R4|C5,O7,R6),F1
	assert(a == true)

	done()
end

local function testOpAndCancelOpTwice(op) -- O2,C5,O7,R6,(O10,R6,)+F1
	case("and cancel op. twice", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		local ok, res = system.suspend(10) -- O7
		assert(not ok)
		assert(res == "canceled")
		a = 2
		local ok, res = system.suspend(10) -- O10
		assert(not ok)
		assert(res == "canceled")
		a = true
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == 2)
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- C5,O7,R6,(O10,R6,)+F1
	assert(a == true)

	done()
end

local function testOpAndReqOp(op) -- O2,C5,O7,F2,C1,Y1
	case("and request op.", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		assert(system.resume(sco)) -- O7
		a = true
	end) -- Y1
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	gc()
	assert(system.run() == false) -- C5,O7,F2,C1,Y1
	assert(a == true)

	done()
end

local function testOpAndThrOp(op) -- O2,C5,O7,F3,C5,Y2,F1
                                  -- O1,C1,O4,C5,Y2,F1
	case("and thread op.", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		assert(system.suspend()) -- O4|O7
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	gc()
	assert(system.run() == false) -- (C1,O4|C5,O7,F3),C5,Y2,F1
	assert(a == true)

	done()
end

local function testOpResumed(op) -- O2,R4,F1
                                 -- O1,R1,C2
	case("cancel schedule", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = true
		coroutine.yield()
		a = false
	end)
	assert(a == 0)

	op:trigger(1)

	assert(coroutine.resume(garbage.co, nil, "canceled")) -- R1|R4
	assert(a == true)

	gc()
	assert(system.run() == false) -- (R1,C2|R4,F1)
	assert(a == true)

	done()
end

local function testOpResumedTwice(op) -- O2,R4,(O10,R6,)+F1
                                      -- O1,R1,(O5,R2,)+C2
	case("cancel schedule twice", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		op:await(2, "fail") -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	assert(coroutine.resume(garbage.co, nil, "canceled")) -- (R1,O5|R4,O10)
	assert(a == 1)

	oncallbacks(function () -- (C5,O6)?
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R2|R6
		assert(a == true)
		op:trigger(2)
	end)

	gc()
	assert(system.run() == false) -- (R1,O5,R2,C2|R4,O10,R6,F1)
	assert(a == true)

	done()
end

local function testOpResumedAndReqOp(op) -- O2,R4,O10,F2,C1,Y1
                                         -- O1,R1,O5,C3,C1,Y1
	case("cancel schedule and request op.", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		system.resume(sco) -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	assert(coroutine.resume(garbage.co, nil, "canceled")) -- R1|R4
	assert(a == 1)

	gc()
	assert(system.run() == false) -- (O1,R1,O5,C3|R4,O10,F2),C1,Y1
	assert(a == true)

	done()
end

local function testOpResumedAndThrOp(op) -- O2,R4,O10,F3,C5,Y2,F1
                                         -- O1,R1,O5,C4,C5,Y2,F1
	case("cancel schedule and thread op.", op)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		assert(system.suspend()) -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	assert(coroutine.resume(garbage.co, nil, "canceled")) -- R1|R4
	assert(a == 1)

	gc()

	assert(system.run() == false) -- (R1,O5,C4|R4,O10,F3),C5,Y2,F1
	assert(a == true)

	done()
end

-- Special Cases:
-- O2,R3,C6,F1
-- O2,R3,C6,(O10,R6,)+F1
-- O2,R3,C6,O10,F2,C1,Y1
-- O2,R3,C6,O10,F3,C5,Y2,F1
-- O2,R3,O8,C5,Y2,F1
-- O2,R3,C6,F1

-- Ignored Cases:
-- O2,R3,O8,(C5,O6,)+C5,Y2,F1
-- O2,R3,O8,C5,Y2,(O10,R6,)+F1
-- O2,R3,O8,C5,Y2,O10,F2,C1,Y1
-- O2,R3,O8,C5,Y2,O10,F3,C5,Y2,F1
-- O2,R3,O8,C5,O7,R6,F1
-- O2,R3,O8,C5,O7,R6,(O10,R6,)+F1
-- O2,R3,O8,C5,O7,F2,C1,Y1
-- O2,R3,O8,C5,O7,F3,C5,Y2,F1
-- O2,R3,O8,R4,F1
-- O2,R3,O8,R4,(O10,R6,)+F1
-- O2,R3,O8,R4,O10,F2,C1,Y1
-- O2,R3,O8,R4,O10,F3,C5,Y2,F1
-- O2,R3,(O9,R5,)+C6,F1
-- O2,R3,O9,C7,R6,F1
-- O2,R3,O9,C7,R6,(O10,R6,)+F1
-- O2,R3,O9,C7,F2,C1,Y1
-- O2,R3,O9,C7,F3,C5,Y2,F1

local function testOp(op)
	testOpAndYield(op)
	testOpTwice(op)
	testOpBeforeCancelOp(op)
	testOpBeforeReqOp(op)
	testOpBeforeThrOp(op)
	testOpAndCancelOp(op)
	testOpAndCancelOpTwice(op)
	testOpAndReqOp(op)
	testOpAndThrOp(op)
	testOpResumed(op)
	testOpResumedTwice(op)
	testOpResumedAndReqOp(op)
	testOpResumedAndThrOp(op)
end

local function testOpClosing(op)
	local trigger = op.trigger
	function op:trigger(cfgid)
		local stream = table.remove(self.stream[cfgid])
		spawn(function ()
			system.suspend()
			stream:close()
		end)
	end
	local check = op.check
	function op:check(cfgid, ok, err)
		if not ok then
			assert(err == "closed" or err == "operation canceled")
		end
	end
	casesuffix = "while closing"
	testOp(op)
	casesuffix = nil
	op.trigger = trigger
	op.check = check
end

local function checkAwait(op, cfgid, ...)
	if not cfgid then
		local ok, err = ...
		assert(not ok)
		assert(err == "canceled")
	else
		op:check(cfgid, ...)
	end
end

local function newTestOpCase(op)
	local await = op.await
	function op:await(cfgid, fail)
		return checkAwait(op, not fail and cfgid or nil, await(self, cfgid))
	end
	if op.setup == nil then function op:setup() end end
	if op.teardown == nil then function op:teardown() end end
	if op.trigger == nil then function op:trigger() end end
	if op.check == nil then function op:check(cfgid, ok) assert(ok) end end
	return op
end

-- Thread Operations

do newtest "time" --------------------------------------------------------------
	testOp(newTestOpCase{
		await = function (self, cfgid)
			return system.suspend(cfgid / 100)
		end,
		trigger = function (self, cfgid)
			local future = system.time() + cfgid / 100
			repeat until system.time("updated") >= future
		end,
	})
end

do newtest "suspend" -----------------------------------------------------------
	testOp(newTestOpCase{
		await = function (self, cfgid)
			return system.suspend()
		end,
	})
end

do newtest "signal" ------------------------------------------------------------
	testOp(newTestOpCase{
		signals = { "winresize", "child" },
		await = function (self, cfgid)
			return system.awaitsig(self.signals[cfgid])
		end,
		trigger = function (self, cfgid)
			system.emitsig(system.procinfo("#"), self.signals[cfgid])
		end,
		check = function (self, cfgid, signal)
			assert(signal == self.signals[cfgid])
		end,
	})
end

do newtest "process" ------------------------------------------------------------
	local script = os.tmpname()
	writeto(script, utilschunk..[[
		local path = ...
		waitsignal(path)
	]])
	testOp(newTestOpCase{
		signals = {},
		await = function (self, cfgid)
			local path = os.tmpname()
			table.insert(self.signals, path)
			return system.execute(luabin, script, path)
		end,
		trigger = function (self, cfgid)
			local path = table.remove(self.signals, 1)
			sendsignal(path)
		end,
		check = function (self, cfgid, result, value)
			assert(result == "exit")
			assert(value == 0)
		end,
	})
	os.remove(script)
end

do newtest "channels" ----------------------------------------------------------
	local op = newTestOpCase{
		channels = { channel.create("A"), channel.create("B") },
		await = function (self, cfgid)
			return system.awaitch(self.channels[cfgid], "in")
		end,
		trigger = function (self, cfgid)
			self.channels[cfgid]:sync("out", "Secret Token "..cfgid)
		end,
		check = function (self, cfgid, ok, res)
			assert(ok == true)
			assert(res == "Secret Token "..cfgid)
		end,
	}

	testOp(op)

	casesuffix = "with the same channel"
	local defaultch = channel.create("Default Test Channel")
	op.channels = { defaultch, defaultch }
	testOpTwice(op)

	casesuffix = "allowing cancellation"
	op.trigger = function () end
	op.check = nil
	testOpResumed(op)
	testOpResumedTwice(op)
	testOpResumedAndReqOp(op)
	testOpResumedAndThrOp(op)

	casesuffix = nil
end

-- Request Operations

do newtest "coroutine" --------------------------------------------------------------
	local chunk = utilschunk..[[
		local coroutine = require "coroutine"
		local cfgid, path = ...
		local token = "Secret Token "..cfgid
		repeat
			waitsignal(path)
		until coroutine.yield(token) ~= cfgid
	]]
	testOp(newTestOpCase{
		coroutines = { stateco.load(chunk), stateco.load(chunk) },
		paths = { os.tmpname(), os.tmpname() },
		await = function (self, cfgid)
			assert(io.open(self.paths[cfgid], "w")):close()
			return system.resume(self.coroutines[cfgid], cfgid, self.paths[cfgid])
		end,
		trigger = function (self, cfgid)
			sendsignal(self.paths[cfgid])
		end,
		check = function (self, cfgid, ok, res)
			assert(ok == true)
			assert(res == "Secret Token "..cfgid)
		end,
	})
end

do newtest "findaddr" ----------------------------------------------------------
	testOp(newTestOpCase{
		names = { "localhost", "ip6-localhost" },
		await = function (self, cfgid)
			return system.findaddr(self.names[cfgid])
		end,
		check = function (self, cfgid, addrlist)
			assert(type(addrlist) == "userdata")
			assert(addrlist:close())
		end,
	})
end

do newtest "nameaddr" ----------------------------------------------------------
	testOp(newTestOpCase{
		address = {
			system.address("ipv4", "127.0.0.1:80"),
			system.address("ipv6", "[::1]:80"),
		},
		await = function (self, cfgid)
			return system.nameaddr(self.address[cfgid])
		end,
		check = function (self, cfgid, hostname, servname)
			assert(string.find(hostname, "localhost"))
			assert(servname == "http")
		end,
	})
end

local function setuptcp(self)
	spawn(function ()
		self.passive = assert(system.socket("passive", "ipv4"))
		assert(self.passive:bind(addr[1]))
		assert(self.passive:listen(2))
		for key, list in pairs(self.stream) do
			table.insert(list, assert(self.passive:accept()))
		end
	end)
	spawn(function ()
		for key, list in pairs(self.stream) do
			local stream = assert(system.socket("stream", "ipv4"))
			assert(stream:connect(addr[1]))
			table.insert(list, stream)
		end
	end)
	assert(system.run() == false)
	self.stream.used = {}
end

local function teardowntcp(self)
	if self.passive ~= nil then
		assert(self.passive:close())
	end
	for _, list in pairs(self.stream) do
		for key, socket in pairs(list) do
			assert(socket:close())
			list[key] = nil
		end
	end
	assert(system.run() == false)
end

local function picktcp(self, cfgid)
	local stream = assert(table.remove(self.stream[cfgid]))
	table.insert(self.stream.used, stream)
	return stream
end

do newtest "shutdown" -----------------------------------------------------------
	testOp(newTestOpCase{
		stream = { {}, {} },
		setup = setuptcp,
		teardown = teardowntcp,
		await = function (self, cfgid)
			return picktcp(self, cfgid):shutdown()
		end,
	})
end

do newtest "udpsend" -----------------------------------------------------------
	testOp(newTestOpCase{
		sockets = {},
		setup = function (self)
			for i = 1, 2 do
				self.sockets[i] = system.socket("datagram", "ipv4")
			end
		end,
		teardown = function (self)
			for index, socket in ipairs(self.sockets) do
				assert(socket:close())
				self.sockets[index] = nil
			end
			assert(system.run() == false)
		end,
		await = function (self, cfgid)
			return self.sockets[cfgid]:write("Hello!", 1, -1, addr[1])
		end,
	})
end

for title, domain in pairs{ tcp = "ipv4", pipe = "local" } do
	local addr = addr
	if domain == "local" then
		addr = { os.tmpname(), os.tmpname() }
		os.remove(addr[1])
		os.remove(addr[2])
	end

	newtest(title.."send") -------------------------------------------------------
	local op = newTestOpCase{
		stream = { {}, {} },
		setup = setuptcp,
		teardown = teardowntcp,
		await = function (self, cfgid)
			return self.stream[cfgid][2]:write("Hello from "..cfgid)
		end,
	}
	testOp(op)
	testOpClosing(op)

	newtest(title.."conn") -------------------------------------------------------
	local op = newTestOpCase{
		stream = { {}, {} },
		setup = function (self)
			for key, list in pairs(self.stream) do
				local passive = assert(system.socket("passive", domain))
				assert(passive:bind(addr[key]))
				assert(passive:listen(0))
				list[1] = passive
				list[2] = assert(system.socket("stream", domain))
			end
		end,
		teardown = teardowntcp,
		await = function (self, cfgid)
			local stream = self.stream[cfgid][2]
			local address = self.stream[cfgid][1]:getaddress()
			return stream:connect(address)
		end,
		trigger = function (self, cfgid)
			local passive = self.stream[cfgid][1]
			local acceptor
			spawn(function ()
				acceptor = coroutine.running()
				local stream = passive:accept()
				if stream then assert(stream:close()) end
			end)
			spawn(function ()
				system.suspend(.1)
				coroutine.resume(acceptor, nil, "timeout")
			end)
		end,
	}
	testOp(op)
	testOpClosing(op)

	-- Object Operations

	newtest(title.."accept") -----------------------------------------------------
	local op = newTestOpCase{
		stream = { {}, {} },
		setup = function (self)
			for key, list in pairs(self.stream) do
				list[1] = assert(system.socket("stream", domain))
				local passive = assert(system.socket("passive", domain))
				assert(passive:bind(addr[key]))
				assert(passive:listen(1))
				list[2] = passive
			end
		end,
		teardown = teardowntcp,
		await = function (self, cfgid)
			local result, errmsg = self.stream[cfgid][2]:accept()
			if result then assert(result:close()) end
			return result, errmsg
		end,
		trigger = function (self, cfgid)
			spawn(function ()
				local stream = self.stream[cfgid][1]
				assert(stream:connect(self.stream[cfgid][2]:getaddress()))
			end)
		end,
	}
	testOp(op)
	testOpClosing(op)

	newtest(title.."recv") -------------------------------------------------------
	local op = newTestOpCase{
		stream = { {}, {} },
		setup = setuptcp,
		teardown = teardowntcp,
		await = function (self, cfgid)
			local buffer = memory.create(6)
			return self.stream[cfgid][2]:read(buffer)
		end,
		trigger = function (self, cfgid)
			spawn(function ()
				self.stream[cfgid][1]:write("Hello"..cfgid)
			end)
		end,
		check = function (self, cfsgid, bytes)
			assert(bytes == 6)
		end,
	}
	testOp(op)
	testOpClosing(op)
end

do newtest "udprecv" ------------------------------------------------------------
	testOp(newTestOpCase{
		sockets = {},
		setup = function (self)
			for i = 1, 2 do
				self.sockets[i] = system.socket("datagram", "ipv4")
				assert(self.sockets[i]:bind(addr[i]))
			end
		end,
		teardown = function (self)
			for index, socket in ipairs(self.sockets) do
				assert(socket:close())
				self.sockets[index] = nil
			end
			assert(system.run() == false)
		end,
		await = function (self, cfgid)
			local buffer = memory.create(6)
			return self.sockets[cfgid]:read(buffer)
		end,
		trigger = function (self, cfgid)
			spawn(function ()
				local address = self.sockets[cfgid]:getaddress()
				self.sockets[3-cfgid]:write("Hello!", 1, 6, address)
			end)
		end,
		check = function (self, cfsgid, bytes)
			assert(bytes == 6)
		end,
	})
end
