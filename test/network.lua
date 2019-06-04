local system = require "coutil.system"

newgroup "Sockets" -----------------------------------------------------------------

newtest "create" ---------------------------------------------------------------

do case "error messages"
	for value in pairs(types) do
		asserterr("bad argument", pcall(system.socket, value))
		asserterr("bad argument", pcall(system.socket, nil, value))
		asserterr("bad argument", pcall(system.socket, nil, value))
	end

	done()
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
	ipaddr[domain].freeaddress = system.address(domain, ipaddr[domain].localhost, 43210)
	ipaddr[domain].localaddress = system.address(domain, ipaddr[domain].localhost, 43212)
	ipaddr[domain].usedaddress = system.address(domain, ipaddr[domain].localhost, 54321)
	ipaddr[domain].deniedaddress = system.address(domain, ipaddr[domain].localhost, 1)
	ipaddr[domain].refusedaddress = system.address(domain, ipaddr[domain].dnshost1, 80)
	ipaddr[domain].remotetcp = system.address(domain, ipaddr[domain].dnshost1, 53)
	ipaddr[domain].othertcp = system.address(domain, ipaddr[domain].dnshost2, 53)
end

for domain, otherdomain in pairs{ipv6 = "ipv4", ipv4 = "ipv6"} do

	newgroup(domain) -------------------------------------------------------------

	for _, kind in ipairs{"datagram", "stream", "listen"} do

		newtest(kind) --------------------------------------------------------------

		do case "garbage generation"
			garbage.sock = assert(system.socket(kind, domain))

			done()
		end

		do case "garbage collection"
			garbage.sock = assert(system.socket(kind, domain))

			gc()
			assert(garbage.sock == nil)

			assert(system.run() == false)

			done()
		end

		do case "close"
			garbage.sock = assert(system.socket(kind, domain))

			assert(garbage.sock:close() == true)

			local protoname = kind == "datagram" and "udp" or "tcp"
			assert(tostring(garbage.sock) == protoname.." (closed)")
			asserterr("closed "..protoname, pcall(garbage.sock.getaddress, garbage.sock))

			assert(garbage.sock:close() == false)

			gc()
			assert(garbage.sock ~= nil)

			assert(system.run() == false)

			done()
		end

		do case "getdomain"
			local sock = assert(system.socket(kind, domain))
			assert(sock:getdomain() == domain)

			done()
		end

		do case "getaddress"
			local sock = assert(system.socket(kind, domain))

			asserterr("wrong domain", pcall(sock.bind, sock, ipaddr[otherdomain].freeaddress))
			assert(sock:bind(ipaddr[domain].freeaddress) == true)
			--asserterr("invalid operation", pcall(sock.bind, sock, ipaddr[domain].localaddress))

			local addr = sock:getaddress()
			assert(addr == ipaddr[domain].freeaddress)
			local addr = sock:getaddress("this")
			assert(addr == ipaddr[domain].freeaddress)
if kind ~= "datagram" then
			asserterr("socket is not connected", sock:getaddress("peer"))
end

			local a = sock:getaddress(nil, addr)
			assert(rawequal(a, addr))
			assert(a == ipaddr[domain].freeaddress)
			local a = sock:getaddress("this", addr)
			assert(rawequal(a, addr))
			assert(a == ipaddr[domain].freeaddress)
if kind ~= "datagram" then
			asserterr("socket is not connected", sock:getaddress("peer", addr))
end

			done()
		end

	end

	newtest "options"

	do case "errors"
		local stream = assert(system.socket("stream", domain))

		asserterr("string expected, got no value", pcall(stream.getoption, stream))
		asserterr("string expected, got no value", pcall(stream.setoption, stream))
		asserterr("value expected", pcall(stream.setoption, stream, "keepalive"))
		asserterr("invalid option", pcall(stream.getoption, stream, "KeepAlive"))
		asserterr("invalid option", pcall(stream.setoption, stream, "KeepAlive", 1))
		asserterr("invalid argument", stream:setoption("keepalive", 0))
		asserterr("number has no integer representation",
			pcall(stream.setoption, stream, "keepalive", 0.123))

		done()
	end

	local function testbooloption(kind, name)
		local sock = assert(system.socket(kind, domain))
		assert(sock:getoption(name) == false)
		assert(sock:setoption(name, true) == true)
		assert(sock:getoption(name) == true)
		assert(sock:setoption(name, false) == true)
	end

	local sockoptions = {
		datagram = {"broadcast", "mcastloop"},
		stream = {"nodelay"},
	}
	for kind, options in pairs(sockoptions) do
		for _, option in ipairs(options) do
			do case(option)
				testbooloption(kind, option)
				done()
			end
		end
	end

	do case "mcastttl"
		local stream = assert(system.socket("datagram", domain))
		assert(stream:getoption("mcastttl") == 1)
		assert(stream:setoption("mcastttl", 123) == true)
		assert(stream:getoption("mcastttl") == 123)
		assert(stream:setoption("mcastttl", 3) == true)
		assert(stream:getoption("mcastttl") == 3)

		done()
	end

	do case "keepalive"
		local stream = assert(system.socket("stream", domain))
		assert(stream:getoption("keepalive") == nil)
		assert(stream:setoption("keepalive", 123) == true)
		assert(stream:getoption("keepalive") == 123)
		assert(stream:setoption("keepalive", false) == true)
		assert(stream:getoption("keepalive") == nil)

		done()
	end

	newtest "listen"

	local backlog = 3

	do case "errors"
		local listen = assert(system.socket("listen", domain))

		assert(listen.setoption == nil)
		assert(listen.getoption == nil)
		assert(listen.connect == nil)
		assert(listen.send == nil)
		assert(listen.receive == nil)
		assert(listen.shutdown == nil)

		asserterr("not listening", pspawn(listen.accept, listen))
		asserterr("number expected, got no value", pcall(listen.listen, listen))
		asserterr("large backlog", pcall(listen.listen, listen, -1))
		asserterr("large backlog", pcall(listen.listen, listen, 1<<31))
		assert(listen:listen(backlog))
		asserterr("already listening", pcall(listen.listen, listen, backlog))
		asserterr("unable to yield", pcall(listen.accept, listen))

		done()
	end

	do case "canceled listen"
		do
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
		end
		gc()

		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))
		assert(server:close())

		garbage.server = assert(system.socket("listen", domain))
		assert(garbage.server:bind(ipaddr[domain].localaddress))
		assert(garbage.server:listen(backlog))

		done()
	end

	do case "successful connections"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			for i = 1, 3 do
				local stream = assert(server:accept())
				assert(stream:close())
				stage = i
			end
		end)
		assert(stage == 0)

		local connected = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.socket("stream", domain))
				assert(stream:connect(ipaddr[domain].localaddress))
				assert(stream:close())
				connected[i] = true
			end)
		end
		assert(stage == 0)
		for i = 1, 3 do
			assert(connected[i] == nil)
		end

		gc()
		assert(system.run() == false)
		assert(stage == 3)
		for i = 1, 3 do
			assert(connected[i] == true)
		end
		assert(server:close())

		done()
	end

	do case "accept transfer"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local thread
		spawn(function ()
			local stream = assert(server:accept())
			assert(stream:close())
			coroutine.resume(thread)
		end)

		local complete = false
		spawn(function ()
			asserterr("already used", pcall(server.accept, server))
			thread = coroutine.running()
			coroutine.yield()
			local stream = assert(server:accept())
			assert(stream:close())
			complete = true
		end)

		spawn(function ()
			for i = 1, 2 do
				local stream = assert(system.socket("stream", domain))
				assert(stream:connect(ipaddr[domain].localaddress))
				assert(stream:close())
			end
		end)

		gc()
		assert(complete == false)
		assert(system.run() == false)
		assert(complete == true)
		assert(server:close())
		thread = nil

		done()
	end

	do case "accumulated connects"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.socket("stream", domain))
				stage[i] = 0
				assert(stream:connect(ipaddr[domain].localaddress))
				stage[i] = 1
				assert(stream:close())
				stage[i] = 2
			end)
			assert(stage[i] == 0)
		end

		local accepted = false
		spawn(function ()
			for i = 1, 3 do
				local stream = assert(server:accept())
				assert(stream:close())
				for j = 1, 3 do
					assert(stage[j] == 0)
				end
			end
			assert(server:close())
			accepted = true
		end)
		assert(accepted == false)

		gc()
		assert(system.run() == false)
		assert(accepted == true)
		for i = 1, 3 do
			assert(stage[i] == 2)
		end

		done()
	end

	do case "accumulated transfers"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.socket("stream", domain))
				stage[i] = 0
				assert(stream:connect(ipaddr[domain].localaddress))
				stage[i] = 1
				assert(stream:close())
				stage[i] = 2
			end)
			assert(stage[i] == 0)
		end

		local accepted = false
		local function acceptor(count)
			if count > 0 then
				local stream = assert(server:accept())
				assert(stream:close())
				for j = 1, 3 do
					assert(stage[j] == 0)
				end
				spawn(acceptor, count-1)
			else
				assert(server:close())
				accepted = true
			end
		end
		spawn(acceptor, 3)
		assert(accepted == false)

		gc()
		assert(system.run() == false)
		assert(accepted == true)
		for i = 1, 3 do
			assert(stage[i] == 2)
		end

		done()
	end

	do case "different accepts"
		local complete = false
		spawn(function ()
			local server1 = assert(system.socket("listen", domain))
			assert(server1:bind(ipaddr[domain].localaddress))
			assert(server1:listen(backlog))

			local server2 = assert(system.socket("listen", domain))
			assert(server2:bind(ipaddr[domain].freeaddress))
			assert(server2:listen(backlog))

			local stream = assert(server1:accept())
			assert(stream:close())

			local stream = assert(server2:accept())
			assert(stream:close())

			assert(server1:close())
			assert(server2:close())
			complete = true
		end)
		assert(complete == false)

		for i, addrname in ipairs{"localaddress", "freeaddress"} do
			spawn(function ()
				local stream = assert(system.socket("stream", domain))
				assert(stream:connect(ipaddr[domain][addrname]))
				assert(stream:close())
			end)
		end

		gc()
		assert(system.run() == false)
		assert(complete == true)

		done()
	end

	do case "close in accept"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage1 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			asserterr("closed", server:accept())
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		spawn(function ()
			system.pause()
			stage2 = 1
			assert(server:close())
			stage2 = 2
		end)
		assert(stage2 == 0)

		gc()
		assert(system.run() == false)
		assert(stage1 == 1)
		assert(stage2 == 2)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			local stream, a,b,c = server:accept()
			assert(stream == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro, garbage, true,nil,3)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel and reschedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream, extra = server:accept()
			assert(stream == nil)
			assert(extra == nil)
			stage = 2
			local stream = assert(server:accept())
			assert(stream:close())
			stage = 3
		end)
		assert(stage == 1)

		coroutine.resume(garbage.coro)
		assert(stage == 2)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			assert(stream:close())
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream, extra = server:accept()
			assert(stream == nil)
			assert(extra == nil)
			stage = 2
			local a,b,c = server:accept()
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 3
		end)
		assert(stage == 1)

		spawn(function ()
			system.pause()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3)
			assert(stage == 3)
			stage = 4
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 2)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "ignore errors"

		local stage = 0
		pspawn(function ()
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:close())
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			assert(stream:close())
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			assert(server:accept() == garbage)
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		coroutine.resume(garbage.coro, garbage)
		assert(stage == 2)

		gc()
		assert(system.run() == false)

		done()
	end

	newtest "connect"

	do case "errors"
		local stream = assert(system.socket("stream", domain))

		assert(stream.listen == nil)
		assert(stream.accept == nil)

		local stage = 0
		spawn(function ()
			system.pause()
			local stream = assert(system.socket("stream", domain))
			stage = 1
			asserterr("netaddress expected", pcall(stream.connect, stream))
			stage = 2
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "successful connection"
		local connected
		spawn(function ()
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			connected = false
			assert(server:accept())
			connected = true
		end)
		assert(connected == false)

		local stage = 0
		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			stage = 1
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
		assert(connected == true)

		done()
	end

	while false do case "connection refused"
		local stage = 0
		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			asserterr("Connection refused", stream:connect(ipaddr[domain].deniedaddress))
			assert(stream:close())
			stage = 1
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(system.socket("stream", domain))
			local a,b,c = stream:connect(ipaddr[domain].localaddress)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro, true,nil,3)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel and reschedule"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(system.socket("stream", domain))
			local a,b = stream:connect(ipaddr[domain].localaddress)
			assert(a == nil)
			assert(b == nil)
			stage = 1
			assert(stream:connect(ipaddr[domain].localaddress))
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		local connected
		spawn(function ()
			assert(server:accept())
			connected = true
		end)
		assert(connected == nil)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		server:close()

		done()
	end

	do case "resume while closing"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress) == nil)
			stage = 1
			local a,b,c = stream:connect(ipaddr[domain].localaddress)
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 2
		end)
		assert(stage == 0)

		spawn(function ()
			system.pause()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
			assert(stage == 2)
			stage = 3
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 3)
		assert(server:close())

		done()
	end

	do case "ignore errors"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			stage = 1
			error("oops!")
		end)
		assert(stage == 0)

		spawn(function ()
			system.pause()
			assert(server:accept())
			assert(server:close())
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "ignore errors after cancel"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress) == nil)
			stage = 1
			error("oops!")
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
		assert(server:close())

		done()
	end

	local memory = require "memory"

	local create = {
		datagram = {
			listen = function (address)
				return { address = address, close = function () return true end }
			end,
			accepted = function (server)
				local sock = assert(system.socket("datagram", domain))
				assert(sock:bind(server.address))
				return sock
			end,
			connected = function (address)
				local sock = assert(system.socket("datagram", domain))
				assert(sock:connect(address))
				return sock
			end,
		},
		stream = {
			listen = function (address)
				local server = assert(system.socket("listen", domain))
				assert(server:bind(address))
				assert(server:listen(backlog))
				return server
			end,
			accepted = function (server)
				return assert(server:accept())
			end,
			connected = function (address)
				local stream = assert(system.socket("stream", domain))
				assert(stream:connect(address))
				return stream
			end,
		},
	}
	for kind, create in pairs(create) do

		newgroup(kind) -----------------------------------------------------------

		newtest "receive"

		do case "errors"
			local server = create.listen(ipaddr[domain].localaddress)

			local address
			local buffer = memory.create(64)

			local stage1 = 0
			local accepted, stream
			spawn(function ()
				stage1 = 1
				accepted = create.accepted(server)
				stage1 = 2
				asserterr("memory expected", pcall(accepted.receive, accepted))
				asserterr("number has no integer representation",
					pcall(accepted.receive, accepted, buffer, 1.1))
				asserterr("number has no integer representation",
					pcall(accepted.receive, accepted, buffer, nil, 2.2))
				assert(accepted:receive(buffer) == 64)
				assert(stream:close() == true)
				stage1 = 3
			end)
			assert(stage1 == (kind == "datagram" and 2 or 1))

			local stage2 = 0
			spawn(function ()
				stage2 = 1
				stream = create.connected(ipaddr[domain].localaddress)
				address = assert(stream:getaddress("this"))
				stage2 = 2
				asserterr("already in use", pcall(accepted.receive, accepted, buffer))
				stage2 = 3
				assert(stream:send(string.rep("x", 64)) == true)
				asserterr("closed", stream:receive(buffer))
				stage2 = 4
				system.pause()
				assert(accepted:close() == true)
				stage2 = 5
			end)
			assert(stage2 == (kind == "datagram" and 3 or 1))

			assert(system.run("step") == true)
			assert(stage1 == (kind == "datagram" and 3 or 2))
			assert(stage2 == (kind == "datagram" and 4 or 3))

			asserterr("unable to yield", pcall(accepted.receive, accepted, buffer))

			gc()
			assert(system.run() == false)
			assert(stage1 == 3)
			assert(stage2 == 5)

			assert(server:close() == true)

			done()
		end

		do case "successful transmissions"
			local server = create.listen(ipaddr[domain].localaddress)

			local data = string.rep("a", 64)
			           ..string.rep("b", 32)
			           ..string.rep("c", 32)

			local done1
			spawn(function ()
				local accepted = create.accepted(server)
				assert(server:close())
				local buffer = memory.create(128)
				local bytes, truncated = accepted:receive(buffer)
				assert(bytes == 64)
				if kind == "datagram" then
					assert(truncated == false)
				else
					assert(truncated == nil)
				end
				assert(memory.diff(buffer, string.sub(data, 1, 64)..string.rep("\0", 64)) == nil)
				local bytes, truncated = accepted:receive(buffer, 65, 96)
				assert(bytes == 32)
				assert(memory.diff(buffer, string.sub(data, 1, 96)..string.rep("\0", 32)) == nil)
				if kind == "datagram" then
					assert(truncated == true)
				else
					assert(truncated == nil)
					assert(accepted:receive(buffer, 97) == 32)
					assert(memory.diff(buffer, data) == nil)
				end
				--assert(accepted:close())
				done1 = true
			end)
			assert(done1 == nil)

			local done2
			spawn(function ()
				local connected = create.connected(ipaddr[domain].localaddress)
				assert(connected:send(data, 1, 64))
				system.pause()
				assert(connected:send(data, 65, 128))
				--assert(connected:close())
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
			local server = create.listen(ipaddr[domain].localaddress)

			local size = 32
			local buffer = memory.create(size)

			local stream, thread
			spawn(function ()
				stream = create.accepted(server)
				assert(server:close())
				assert(stream:receive(buffer) == size)
				assert(memory.diff(buffer, string.rep("a", size)) == nil)
				coroutine.resume(thread)
			end)

			local complete = false
			spawn(function ()
				thread = coroutine.running()
				coroutine.yield()
				asserterr("already in use", pcall(stream.receive, stream))
				coroutine.yield()
				assert(stream:receive(buffer) == size)
				assert(memory.diff(buffer, string.rep("b", size)) == nil)
				assert(stream:close())
				complete = true
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				coroutine.resume(thread)
				assert(stream:send(string.rep("a", size)))
				assert(stream:send(string.rep("b", size)))
				assert(stream:close())
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
			for i, addrname in ipairs{"localaddress", "freeaddress"} do
				spawn(function ()
					local server = create.listen(ipaddr[domain][addrname])

					local stream = create.accepted(server)
					spawn(function ()
						local buffer = memory.create(64)
						assert(stream:receive(buffer) == 32)
						memory.fill(buffer, buffer, 33, 64)
						assert(stream:send(buffer))
						assert(stream:close())
					end)

					assert(server:close())
				end)

				spawn(function ()
					local stream = create.connected(ipaddr[domain][addrname])
					assert(stream:send(string.rep("x", 32)))
					local buffer = memory.create(64)
					assert(stream:receive(buffer) == 64)
					assert(stream:close())
					assert(memory.diff(buffer, string.rep("x", 64)) == nil)
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
				local server = create.listen(ipaddr[domain].localaddress)
				local stream = create.accepted(server)
				local res, a,b,c = stream:receive(memory.create(10))
				assert(res == garbage)
				assert(a == true)
				assert(b == nil)
				assert(c == 3)
				stage = 1
				coroutine.yield()
				stage = 2
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
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
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				local buffer = memory.create("9876543210")
				local bytes, extra = stream:receive(buffer)
				assert(bytes == nil)
				assert(extra == nil)
				assert(memory.diff(buffer, "9876543210") == nil)
				stage = 2
				assert(stream:receive(buffer) == 10)
				assert(memory.diff(buffer, "0123456789") == nil)
				assert(stream:close())
				stage = 3
			end)
			assert(stage == 1)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro)
				assert(stage == 2)
				assert(stream:send("0123456789"))
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
				local server = create.listen(ipaddr[domain].localaddress)
				local stream = create.accepted(server)
				local buffer = memory.create("9876543210")
				stage = 1
				local bytes, extra = stream:receive(buffer)
				assert(bytes == nil)
				assert(extra == nil)
				stage = 2
				local a,b,c = stream:receive(buffer)
				assert(a == .1)
				assert(b == 2.2)
				assert(c == 33.3)
				stage = 3
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro)
				assert(stage == 2)
				system.pause()
				coroutine.resume(garbage.coro, .1, 2.2, 33.3)
				assert(stage == 3)
				stage = 4
			end)
			assert(stage == 0)

			gc()
			assert(system.run() == false)
			assert(stage == 4)

			done()
		end

		do case "ignore errors"

			local stage = 0
			pspawn(function ()
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				local buffer = memory.create("9876543210")
				assert(stream:receive(buffer) == 10)
				assert(memory.diff(buffer, "0123456789") == nil)
				stage = 2
				error("oops!")
			end)
			assert(stage == 1)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				assert(stream:send("0123456789"))
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
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				assert(stream:receive(memory.create(10)) == garbage)
				stage = 2
				error("oops!")
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
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
			local server = create.listen(ipaddr[domain].localaddress)

			local data = string.rep("x", 1<<24)

			local stage1 = 0
			local accepted, stream
			spawn(function ()
				stage1 = 1
				accepted = create.accepted(server)
				stage1 = 2
				asserterr("memory expected", pcall(accepted.send, accepted))
				asserterr("number has no integer representation",
					pcall(accepted.send, accepted, data, 1.1))
				asserterr("number has no integer representation",
					pcall(accepted.send, accepted, data, nil, 2.2))
				stage1 = 3
				asserterr("connection reset by peer", accepted:send(data))
				stage1 = 4
			end)
			assert(stage1 == 1)

			local stage2 = 0
			spawn(function ()
				stage2 = 1
				stream = create.connected(ipaddr[domain].localaddress)
				stage2 = 2
				system.pause()
				stage2 = 3
				assert(stream:close())
				stage2 = 4
			end)
			assert(stage2 == 1)

			assert(system.run("step") == true)
			assert(stage1 == 3)
			assert(stage2 == 2)

			asserterr("unable to yield", pcall(accepted.send, accepted, buffer))

			gc()
			assert(system.run() == false)
			assert(stage1 == 4)
			assert(stage2 == 4)

			assert(server:close() == true)

			done()
		end

		do case "cancel schedule"

			local stage = 0
			spawn(function ()
				garbage.coro = coroutine.running()
				local server = create.listen(ipaddr[domain].localaddress)
				local stream = create.accepted(server)
				local res, a,b,c = stream:send(string.rep("x", 1<<24))
				assert(res == garbage)
				assert(a == true)
				assert(b == nil)
				assert(c == 3)
				stage = 1
				coroutine.yield()
				stage = 2
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro, garbage, true,nil,3)

				local bytes = 1<<24
				local buffer = memory.create(1<<24)
				repeat
					bytes = bytes-stream:receive(buffer)
				until bytes <= 0
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
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				local ok, extra = stream:send(string.rep("x", 1<<24))
				assert(ok == nil)
				assert(extra == nil)
				stage = 3
				assert(stream:send(string.rep("x", 64)) == true)
				stage = 4
				assert(stream:close())
				stage = 5
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro)
				local bytes = (1<<24)+64
				local buffer = memory.create(bytes)
				repeat
					bytes = bytes-stream:receive(buffer)
				until bytes <= 0
			end)
			assert(stage == 1)

			gc()
			assert(system.run() == false)
			assert(stage == 5)

			done()
		end

		do case "double cancel"
			local stage = 0
			spawn(function ()
				garbage.coro = coroutine.running()
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				local ok, extra = stream:send(string.rep("x", 1<<24))
				assert(ok == nil)
				assert(extra == nil)
				stage = 3
				local res, a,b,c = stream:send(string.rep("x", 1<<24))
				assert(res == garbage)
				assert(a == true)
				assert(b == nil)
				assert(c == 3)
				stage = 3

				assert(stream:close())
				stage = 4
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro)
				local bytes = 1<<24
				local buffer = memory.create(1<<24)
				repeat
					bytes = bytes-stream:receive(buffer)
				until bytes <= 0
				coroutine.resume(garbage.coro, garbage, true,nil,3)
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
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				assert(stream:send("0123456789") == true)
				stage = 2
				error("oops!")
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				local buffer = memory.create("9876543210")
				assert(stream:receive(buffer) == 10)
				assert(memory.diff(buffer, "0123456789") == nil)
				assert(stage == 2)
				stage = 3
			end)
			assert(stage == 1)

			gc()
			assert(system.run() == false)
			assert(stage == 3)

			done()
		end

		do case "ignore errors after cancel"

			local stage = 0
			pspawn(function ()
				garbage.coro = coroutine.running()
				local server = create.listen(ipaddr[domain].localaddress)
				stage = 1
				local stream = create.accepted(server)
				assert(stream:send(string.rep("x", 1<<24)) == garbage)
				stage = 2
				error("oops!")
			end)

			spawn(function ()
				local stream = create.connected(ipaddr[domain].localaddress)
				system.pause()
				coroutine.resume(garbage.coro, garbage)

				assert(stream:close())
			end)
			assert(stage == 1)

			gc()
			assert(system.run() == false)
			assert(stage == 2)

			done()
		end

	end

	local backlog = 3

end