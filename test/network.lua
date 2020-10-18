local system = require "coutil.system"

local function testbooloption(sock, name)
	assert(sock:setoption(name, true) == true)
	assert(sock:setoption(name, false) == true)
end

local ipaddr = {
	ipv4 = {
		localhost = "127.0.0.1",
		dnshost1 = "8.8.8.8",
		dnshost2 = "8.8.4.4",
	},
	ipv6 = {
		localhost = "::1",
		dnshost1 = "2001:4860:4860::8888",
		dnshost2 = "2001:4860:4860::8844",
	},
}
for _, domain in ipairs{ "ipv4", "ipv6" } do
	ipaddr[domain].free = system.address(domain, ipaddr[domain].localhost, 43210)
	ipaddr[domain].bindable = system.address(domain, ipaddr[domain].localhost, 43212)
	ipaddr[domain].used = system.address(domain, ipaddr[domain].localhost, 54321)
	ipaddr[domain].denied = system.address(domain, ipaddr[domain].localhost, 1)
	ipaddr[domain].refused = system.address(domain, ipaddr[domain].dnshost1, 80)
	ipaddr[domain].remote = system.address(domain, ipaddr[domain].dnshost1, 53)
	ipaddr[domain].other = system.address(domain, ipaddr[domain].dnshost2, 53)
end

local otherdomain = {
	ipv4 = "ipv6",
	ipv6 = "ipv4",
}
local function testsockaddr(create, domain, ...)
	do case "getdomain"
		local sock = assert(create(...))
		assert(sock:getdomain() == domain)

		done()
	end

	do case "getaddress"
		local sock = assert(create(...))

		local badaddr = ipaddr[otherdomain[domain]].free
		asserterr("wrong domain", pcall(sock.bind, sock, badaddr))
		assert(sock:bind(ipaddr[domain].free) == true)
		asserterr("invalid argument", sock:bind(ipaddr[domain].bindable))

		local addr = sock:getaddress()
		assert(addr == ipaddr[domain].free)
		local addr = sock:getaddress("self")
		assert(addr == ipaddr[domain].free)
		asserterr("socket is not connected", sock:getaddress("peer"))

		local a = sock:getaddress(nil, addr)
		assert(rawequal(a, addr))
		assert(a == ipaddr[domain].free)
		local a = sock:getaddress("self", addr)
		assert(rawequal(a, addr))
		assert(a == ipaddr[domain].free)
		asserterr("socket is not connected", sock:getaddress("peer", addr))

		done()
	end
end

newtest "socket"

do case "error messages"
	for value in pairs(types) do
		asserterr("bad argument", pcall(system.socket, value))
		asserterr("bad argument", pcall(system.socket, value, "ipv4"))
		asserterr("bad argument", pcall(system.socket, value, "ipv6"))
		asserterr("bad argument", pcall(system.socket, value, "local"))
		asserterr("bad argument", pcall(system.socket, value, "share"))
		asserterr("bad argument", pcall(system.socket, "datagram", value))
		asserterr("bad argument", pcall(system.socket, "stream", value))
		asserterr("bad argument", pcall(system.socket, "passive", value))
		asserterr("socket type not supported", system.socket("datagram", "local"))
		asserterr("socket type not supported", system.socket("datagram", "share"))
	end

	done()
end

for _, domain in ipairs{ "ipv4", "ipv6" } do

	newgroup("udp:"..domain) -----------------------------------------------------

	local function create() return system.socket("datagram", domain) end

	newtest "creation"

	testobject(create)

	newtest "address"

	testsockaddr(create, domain)

	newtest "options"

	do case "errors"
		local socket = assert(create())

		asserterr("string expected, got no value", pcall(socket.setoption, socket))
		asserterr("number expected, got no value", pcall(socket.setoption, socket, "mcastttl"))
		asserterr("invalid option", pcall(socket.setoption, socket, "MCastTTL", 1))

		done()
	end

	for _, option in ipairs{ "broadcast", "mcastloop" } do
		do case(option)
			testbooloption(create(), option)
			done()
		end
	end

	do case "mcastttl"
		local stream = assert(create())
		for _, value in ipairs{ 1, 2, 3, 123, 128, 255 } do
			assert(stream:setoption("mcastttl", value) == true)
		end

		for _, value in ipairs{ -1, 0, 256, 257, math.maxinteger } do
			asserterr("must be from 1 upto 255",
				pcall(stream.setoption, stream, "mcastttl", value))
		end

		done()
	end

	newtest "receive"

	local memory = require "memory"

	local backlog = 3

	do case "errors"
		local buffer = memory.create(64)

		local connected, unconnected
		local stage1 = 0
		spawn(function ()
			stage1 = 1
			unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			stage1 = 2
			asserterr("memory expected", pcall(unconnected.receive, unconnected))
			asserterr("number has no integer representation",
				pcall(unconnected.receive, unconnected, buffer, 1.1))
			asserterr("number has no integer representation",
				pcall(unconnected.receive, unconnected, buffer, nil, 2.2))
			stage1 = 3
			assert(unconnected:receive(buffer) == 64)
			stage1 = 4
			assert(connected:close() == true)
			stage1 = 5
		end)
		assert(stage1 == 2)

		local stage2 = 0
		spawn(function ()
			stage2 = 1
			connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			stage2 = 2
			asserterr("already in use", pcall(unconnected.receive, unconnected, buffer))
			stage2 = 3
			assert(connected:send(string.rep("x", 64)) == true)
			stage2 = 4
			asserterr("closed", connected:receive(buffer))
			stage2 = 5
		end)
		assert(stage2 == 3)

		asserterr("unable to yield", pcall(connected.receive, connected, buffer))

		gc()
		assert(system.run() == false)
		assert(stage1 == 5)
		assert(stage2 == 5)
		assert(unconnected:close() == true)

		done()
	end

	do case "successful transmissions"
		local data = string.rep("a", 64)
		           ..string.rep("b", 32)
		           ..string.rep("c", 32)

		local function assertfilled(buffer, count)
			local expected = string.sub(data, 1, count)..string.rep("\0", #data-count)
			assert(not memory.diff(buffer, expected))
		end

		local done1
		spawn(function ()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(128)
			local bytes, trunced = unconnected:receive(buffer)
			assert(bytes == 64)
			assert(trunced == false)
			assertfilled(buffer, 64)
			local bytes, trunced = unconnected:receive(buffer, 65, 96)
			assert(bytes == 32)
			assert(trunced == true)
			assertfilled(buffer, 96)
			local bytes, trunced = unconnected:receive(buffer, 97)
			assert(bytes == 0)
			assert(trunced == false)
			assertfilled(buffer, 96)
			done1 = true
		end)
		assert(done1 == nil)

		local done2
		spawn(function ()
			local connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			assert(connected:send(data, 1, 64))
			assert(connected:send(data, 65, 128))
			assert(connected:send(data, 129, 128))  -- send empty datagram
			done2 = true
		end)
		assert(done2 == nil)

		gc()
		assert(system.run() == false)
		assert(done1 == true)
		assert(done2 == true)

		done()
	end

	do case "receive transfer"
		local size = 32
		local buffer = memory.create(size)

		local unconnected, thread
		spawn(function ()
			unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			assert(unconnected:receive(buffer) == size)
			assert(not memory.diff(buffer, string.rep("a", size)))
			coroutine.resume(thread)
		end)

		local complete = false
		spawn(function ()
			thread = coroutine.running()
			coroutine.yield()
			asserterr("already in use", pcall(unconnected.receive, unconnected))
			coroutine.yield()
			assert(unconnected:receive(buffer) == size)
			assert(not memory.diff(buffer, string.rep("b", size)))
			assert(unconnected:close())
			complete = true
		end)

		spawn(function ()
			local connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			coroutine.resume(thread)
			assert(connected:send(string.rep("a", size)))
			assert(connected:send(string.rep("b", size)))
			assert(connected:close())
		end)

		gc()
		assert(complete == false)
		assert(system.run() == false)
		assert(complete == true)
		thread = nil

		done()
	end

	do case "multiple threads"
		local complete = {}
		for i, addrname in ipairs{"bindable", "free"} do
			local connected, unconnected
			spawn(function ()
				unconnected = assert(create())
				assert(unconnected:bind(ipaddr[domain][addrname]))
				spawn(function ()
					local buffer = memory.create(64)
					assert(unconnected:receive(buffer) == 32)
					memory.fill(buffer, buffer, 33, 64)
					assert(unconnected:send(buffer, nil, nil, connected:getaddress()))
				end)
			end)

			spawn(function ()
				connected = assert(create())
				assert(connected:connect(ipaddr[domain][addrname]))
				assert(connected:send(string.rep("x", 32)))
				local buffer = memory.create(64)
				local addr = system.address(domain)
				assert(connected:receive(buffer, nil, nil, addr) == 64)
				assert(not memory.diff(buffer, string.rep("x", 64)))
				assert(tostring(addr) == tostring(unconnected:getaddress()))
				assert(connected:close())
				assert(unconnected:close())
				complete[i] = true
			end)
			assert(complete[i] == nil)
		end

		gc()
		assert(system.run() == false)
		assert(complete[1] == true)
		assert(complete[2] == true)

		done()
	end

	do case "cancel schedule"

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local res, a,b,c = unconnected:receive(memory.create(10))
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			coroutine.yield()
			stage = 2
		end)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro, garbage, true,nil,3)
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel and reschedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			stage = 1
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create("9876543210")
			local bytes, extra = unconnected:receive(buffer)
			assert(bytes == nil)
			assert(extra == nil)
			assert(not memory.diff(buffer, "9876543210"))
			stage = 2
			assert(unconnected:receive(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			assert(unconnected:close())
			stage = 3
		end)
		assert(stage == 1)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			local connected = assert(create())
			assert(connected:send("0123456789", nil, nil, ipaddr[domain].bindable))
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 3)

		done()
	end

	do case "double cancel"

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create("9876543210")
			stage = 1
			local bytes, extra = unconnected:receive(buffer)
			assert(bytes == nil)
			assert(extra == nil)
			stage = 2
			local a,b,c = unconnected:receive(buffer)
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 3
		end)
		assert(stage == 1)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			system.suspend()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3)
			assert(stage == 3)
			stage = 4
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "ignore errors"

		local stage = 0
		pspawn(function ()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create("9876543210")
			stage = 1
			assert(unconnected:receive(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local unconnected = assert(create())
			assert(unconnected:send("0123456789", nil, nil, ipaddr[domain].bindable))
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "ignore errors after cancel"

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(create())
			stage = 1
			assert(unconnected:receive(memory.create(10)) == garbage)
			stage = 2
			error("oops!")
		end)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro, garbage)
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	newtest "send"

	do case "errors"
		local data = string.rep("x", 1<<24)
		local addr = system.address(domain)

		local complete, connected
		spawn(function ()
			connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			asserterr("memory expected",
				pcall(connected.send, connected))
			asserterr("number has no integer representation",
				pcall(connected.send, connected, data, 1.1))
			asserterr("number has no integer representation",
				pcall(connected.send, connected, data, nil, 2.2))
			complete = true
		end)
		assert(complete == true)

		local complete, unconnected
		spawn(function ()
			unconnected = assert(create())
			asserterr("address expected",
				pcall(unconnected.send, unconnected))
			asserterr("memory expected",
				pcall(unconnected.send, unconnected, nil, nil, nil, addr))
			asserterr("number has no integer representation",
				pcall(unconnected.send, unconnected, data, 1.1, nil, addr))
			asserterr("number has no integer representation",
				pcall(unconnected.send, unconnected, data, nil, 2.2, addr))
			complete = true
		end)
		assert(complete == true)

		asserterr("unable to yield",
			pcall(connected.send, connected, buffer))
		asserterr("unable to yield",
			pcall(unconnected.send, unconnected, buffer))

		gc()
		assert(system.run() == false)

		done()
	end

	do case "cancel schedule"

		local stage1 = 0
		spawn(function ()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(128)
			assert(unconnected:receive(buffer) == 128)
			assert(not memory.diff(buffer, string.rep("x", 128)))
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(create())
			stage2 = 1
			local res, a,b,c = unconnected:send(string.rep("x", 128), nil, nil,
			                                    ipaddr[domain].bindable)
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage2 = 2
			coroutine.yield()
			stage2 = 3
		end)
		assert(stage1 == 0)
		assert(stage2 == 1)

		spawn(function ()
			--system.suspend()
			coroutine.resume(garbage.coro, garbage, true,nil,3)
		end)
		assert(stage1 == 0)
		assert(stage2 == 2)

		gc()
		assert(system.run() == false)
		assert(stage1 == 1)
		assert(stage2 == 2)

		done()
	end

	do case "cancel and reschedule"
		local stage1 = 0
		spawn(function ()
			stage1 = 1
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(128)
			stage1 = 2
			assert(unconnected:receive(buffer) == 128)
			assert(not memory.diff(buffer, string.rep("x", 128)))
			local buffer = memory.create(64)
			assert(unconnected:receive(buffer) == 64)
			assert(not memory.diff(buffer, string.rep("x", 64)))
			stage1 = 3
		end)
		assert(stage1 == 2)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			stage2 = 1
			local connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			stage2 = 2
			local ok, extra = connected:send(string.rep("x", 128))
			assert(ok == nil)
			assert(extra == nil)
			stage2 = 3
			assert(connected:send(string.rep("x", 64)) == true)
			stage2 = 4
			assert(connected:close())
			stage2 = 5
		end)
		assert(stage2 == 2)

		coroutine.resume(garbage.coro)
		assert(stage1 == 2)
		assert(stage2 == 3)

		gc()
		assert(system.run() == false)
		assert(stage1 == 3)
		assert(stage2 == 5)

		done()
	end

	do case "double cancel"
		local stage0 = 0
		spawn(function ()
			stage0 = 1
			system.suspend()
			stage0 = 2
			coroutine.resume(garbage.coro, garbage, true,nil,3)
			stage0 = 3
		end)
		assert(stage0 == 1)

		local stage1 = 0
		spawn(function ()
			stage1 = 1
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(128)
			stage1 = 2
			assert(unconnected:receive(buffer) == 128)
			assert(not memory.diff(buffer, string.rep("x", 128)))
			--local buffer = memory.create(64)
			--assert(unconnected:receive(buffer) == 64)
			--assert(not memory.diff(buffer, string.rep("x", 64)))
			stage1 = 3
		end)
		assert(stage1 == 2)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			stage2 = 1
			local connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			stage2 = 2
			local ok, extra = connected:send(string.rep("x", 128))
			assert(ok == nil)
			assert(extra == nil)
			stage2 = 3
			local res, a,b,c = connected:send(string.rep("x", 64))
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage2 = 4
			assert(connected:close())
			stage2 = 5
		end)
		assert(stage2 == 2)

		coroutine.resume(garbage.coro)
		assert(stage2 == 3)

		gc()
		assert(system.run() == false)
		assert(stage0 == 3)
		assert(stage1 == 3)
		assert(stage2 == 5)

		done()
	end

	do case "ignore errors"

		local stage1 = 0
		pspawn(function ()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(10)
			stage1 = 1
			assert(unconnected:receive(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			stage1 = 2
		end)
		assert(stage1 == 1)

		local stage2 = 0
		pspawn(function ()
			local connected = assert(create())
			assert(connected:connect(ipaddr[domain].bindable))
			stage2 = 1
			assert(connected:send("0123456789"))
			stage2 = 2
			error("oops!")
			stage2 = 3
		end)
		assert(stage1 == 1)
		assert(stage2 == 1)

		gc()
		assert(system.run() == false)
		assert(stage1 == 2)
		assert(stage2 == 2)

		done()
	end

	do case "ignore errors after cancel"

		local stage1 = 0
		spawn(function ()
			local unconnected = assert(create())
			assert(unconnected:bind(ipaddr[domain].bindable))
			local buffer = memory.create(128)
			assert(unconnected:receive(buffer) == 128)
			assert(not memory.diff(buffer, string.rep("x", 128)))
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(create())
			stage2 = 1
			local res, a,b,c = unconnected:send(string.rep("x", 128), nil, nil,
			                                    ipaddr[domain].bindable)
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage2 = 2
			error("oops!")
			stage2 = 3
		end)
		assert(stage1 == 0)
		assert(stage2 == 1)

		spawn(function ()
			--system.suspend()
			coroutine.resume(garbage.coro, garbage, true,nil,3)
		end)
		assert(stage1 == 0)
		assert(stage2 == 2)

		gc()
		assert(system.run() == false)
		assert(stage1 == 1)
		assert(stage2 == 2)

		done()
	end

	newgroup("tcp:"..domain) -----------------------------------------------------

	local function create(kind) return system.socket(kind, domain) end

	newtest "creation"

	testobject(create, "passive")
	testobject(create, "stream")

	newtest "address"

	testsockaddr(create, domain, "passive")
	testsockaddr(create, domain, "stream")

	newtest "options"

	do case "errors"
		local socket = assert(create("stream"))

		asserterr("string expected, got no value", pcall(socket.setoption, socket))
		asserterr("value expected", pcall(socket.setoption, socket, "keepalive"))
		asserterr("invalid option", pcall(socket.setoption, socket, "KeepAlive", 1))

		done()
	end

	do case "nodelay"
		testbooloption(create("stream"), "nodelay")
		done()
	end

	do case "keepalive"
		local stream = assert(create("stream"))
		for _, value in ipairs{ 1, 2, 3, 123, 128, 255 } do
			assert(stream:setoption("keepalive", value) == true)
		end
		assert(stream:setoption("keepalive", false) == true)

		for _, value in ipairs{ -1, 0 } do
			asserterr("invalid argument",
				pcall(stream.setoption, stream, "keepalive", value))
		end

		done()
	end

	teststream(create, ipaddr[domain])

end


do case "used after library collection"
	dostring(utilschunk..[===[
		local system = require "coutil.system"
		local addr = system.address("ipv4", "127.0.0.1:65432")
		local path = os.tmpname()
		local cases = {}
		table.insert(cases, { socket = system.socket("datagram", addr.type), op = "send", "xxx", nil, nil, addr })
		table.insert(cases, { socket = system.socket("stream", addr.type), op = "connect", addr })
		table.insert(cases, { socket = system.socket("stream", "local"), op = "connect", path })
		table.insert(cases, { socket = system.socket("stream", "share"), op = "connect", path })

		garbage.system = system
		system = nil
		package.loaded["coutil.system"] = nil
		gc()
		assert(garbage.system == nil)

		for _, case in ipairs(cases) do
			spawn(function ()
				local ok, err = case.socket[case.op](case.socket, table.unpack(case))
				if case.op == "send" then
					assert(ok)
				else
					assert(not ok)
					assert(err == "operation canceled")
				end
			end)
		end
	]===])

	done()
end
