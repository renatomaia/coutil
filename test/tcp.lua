local system = require "coutil.system"

newgroup "TCP" -----------------------------------------------------------------

newtest "create" ---------------------------------------------------------------

do case "error messages"
	for value in pairs(types) do
		asserterr("bad argument", pcall(system.tcp, value))
		asserterr("bad argument", pcall(system.tcp, nil, value))
		asserterr("bad argument", pcall(system.tcp, nil, value))
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

	for _, kind in ipairs{"stream", "listen"} do

		newtest(kind) --------------------------------------------------------------

		do case "garbage generation"
			garbage.tcp = system.tcp(kind, domain)

			done()
		end

		do case "garbage collection"
			garbage.tcp = system.tcp(kind, domain)

			gc()
			assert(garbage.tcp == nil)

			assert(system.run() == false)

			done()
		end

		do case "close"
			garbage.tcp = system.tcp(kind, domain)

			assert(garbage.tcp:close() == true)
			assert(tostring(garbage.tcp) == "tcp (closed)")
			asserterr("closed tcp", pcall(garbage.tcp.getaddress, garbage.tcp))

			assert(garbage.tcp:close() == false)

			gc()
			assert(garbage.tcp ~= nil)

			assert(system.run() == false)

			done()
		end

		do case "getdomain"
			local tcp = system.tcp(kind, domain)
			assert(tcp:getdomain() == domain)

			done()
		end

		do case "getaddress"
			local tcp = system.tcp(kind, domain)

			asserterr("wrong domain", pcall(tcp.bind, tcp, ipaddr[otherdomain].freeaddress))
			assert(tcp:bind(ipaddr[domain].freeaddress) == true)
			--asserterr("invalid operation", pcall(tcp.bind, tcp, ipaddr[domain].localaddress))

			local addr = tcp:getaddress()
			assert(addr == ipaddr[domain].freeaddress)
			local addr = tcp:getaddress("this")
			assert(addr == ipaddr[domain].freeaddress)
			asserterr("socket is not connected", tcp:getaddress("peer"))

			local a = tcp:getaddress(nil, addr)
			assert(rawequal(a, addr))
			assert(a == ipaddr[domain].freeaddress)
			local a = tcp:getaddress("this", addr)
			assert(rawequal(a, addr))
			assert(a == ipaddr[domain].freeaddress)
			asserterr("socket is not connected", tcp:getaddress("peer", addr))

			done()
		end

	end

	newtest "options"

	do case "errors"
		local stream = system.tcp("stream", domain)

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

	do case "keepalive"
		local stream = system.tcp("stream", domain)
		assert(stream:getoption("keepalive") == nil)
		assert(stream:setoption("keepalive", 123) == true)
		assert(stream:getoption("keepalive") == 123)
		assert(stream:setoption("keepalive", false) == true)
		assert(stream:getoption("keepalive") == nil)

		done()
	end

	do case "nodelay"
		local stream = system.tcp("stream", domain)
		assert(stream:getoption("nodelay") == false)
		assert(stream:setoption("nodelay", true) == true)
		assert(stream:getoption("nodelay") == true)
		assert(stream:setoption("nodelay", false) == true)

		done()
	end

	newtest "listen"

	local backlog = 3

	do case "errors"
		local listen = system.tcp("listen", domain)

		assert(listen.setoption == nil)
		assert(listen.getoption == nil)
		assert(listen.connect == nil)
		assert(listen.send == nil)
		assert(listen.receive == nil)
		assert(listen.shutdown == nil)

		asserterr("unavailable", listen:accept())
		asserterr("number expected, got no value", pcall(listen.listen, listen))
		asserterr("invalid backlog value", pcall(listen.listen, listen, -1))
		asserterr("invalid backlog value", pcall(listen.listen, listen, 1<<31))
		asserterr("invalid backlog value", pcall(listen.listen, listen, 1<<31-1))
		assert(listen:listen(backlog))
		asserterr("already listening", listen:listen(backlog))
		asserterr("unable to yield", pcall(listen.accept, listen))

		done()
	end

	do case "canceled listen"
		do
			local server = system.tcp("listen", domain)
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
		end
		gc()

		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))
		assert(server:close())

		garbage.server = system.tcp("listen", domain)
		assert(garbage.server:bind(ipaddr[domain].localaddress))
		assert(garbage.server:listen(backlog))

		done()
	end

	do case "successful connections"
		local server = system.tcp("listen", domain)
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
				local stream = system.tcp("stream", domain)
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

	do case "accept tranfer"
		local server = system.tcp("listen", domain)
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
			asserterr("unavailable", server:accept())
			thread = coroutine.running()
			coroutine.yield()
			local stream = assert(server:accept())
			assert(stream:close())
			complete = true
		end)

		spawn(function ()
			for i = 1, 2 do
				local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.tcp("stream", domain))
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.tcp("stream", domain))
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
			local server1 = system.tcp("listen", domain)
			assert(server1:bind(ipaddr[domain].localaddress))
			assert(server1:listen(backlog))

			local server2 = system.tcp("listen", domain)
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
				local stream = system.tcp("stream", domain)
				assert(stream:connect(ipaddr[domain][addrname]))
				assert(stream:close())
			end)
		end

		gc()
		assert(system.run() == false)
		assert(complete == true)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = system.tcp("listen", domain)
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
			local server = system.tcp("listen", domain)
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
			local stream = system.tcp("stream", domain)
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
			local server = system.tcp("listen", domain)
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
			local server = assert(system.tcp("listen", domain))
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
			local stream = assert(system.tcp("stream", domain))
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
			local server = assert(system.tcp("listen", domain))
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
		local stream = system.tcp("stream", domain)

		assert(stream.listen == nil)
		assert(stream.accept == nil)

		local stage = 0
		spawn(function ()
			system.pause()
			local stream = system.tcp("stream", domain)
			stage = 1
			asserterr("netaddress expected, got nil", pcall(stream.connect, stream))
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
			local server = system.tcp("listen", domain)
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			connected = false
			assert(server:accept())
			connected = true
		end)
		assert(connected == false)

		local stage = 0
		spawn(function ()
			local stream = system.tcp("stream", domain)
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
			local stream = system.tcp("stream", domain)
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
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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









































	newtest "receive"

	local memory = require "memory"

	do case "errors"
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local buffer = memory.create(64)

		local stage = 0
		local accepted, stream
		spawn(function ()
			stage = 1
			accepted = assert(server:accept())
			asserterr("memory expected", pcall(accepted.receive, accepted))
			stage = 4
			assert(accepted:receive(buffer))
			stage = 5
		end)
		assert(stage == 1)

		spawn(function ()
			stream = system.tcp("stream", domain)
			stage = 2
			assert(stream:connect(ipaddr[domain].localaddress))
			asserterr("already in use", pcall(accepted.receive, accepted, buffer))
			stage = 3
			assert(stream:send(string.rep("x", 64)))
		end)
		assert(stage == 2)

		assert(system.run("step") == true)
		assert(stage == 3)

		asserterr("unable to yield", pcall(accepted.receive, stream, buffer))

		gc()
		assert(system.run() == false)
		assert(stage == 5)

		done()
	end

	do case "canceled listen"
		do
			local server = system.tcp("listen", domain)
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
		end
		gc()

		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))
		assert(server:close())

		garbage.server = system.tcp("listen", domain)
		assert(garbage.server:bind(ipaddr[domain].localaddress))
		assert(garbage.server:listen(backlog))

		done()
	end

	do case "successful connections"
		local server = system.tcp("listen", domain)
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
				local stream = system.tcp("stream", domain)
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

	do case "accept tranfer"
		local server = system.tcp("listen", domain)
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
			asserterr("unavailable", server:accept())
			thread = coroutine.running()
			coroutine.yield()
			local stream = assert(server:accept())
			assert(stream:close())
			complete = true
		end)

		spawn(function ()
			for i = 1, 2 do
				local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.tcp("stream", domain))
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(system.tcp("stream", domain))
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
			local server1 = system.tcp("listen", domain)
			assert(server1:bind(ipaddr[domain].localaddress))
			assert(server1:listen(backlog))

			local server2 = system.tcp("listen", domain)
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
				local stream = system.tcp("stream", domain)
				assert(stream:connect(ipaddr[domain][addrname]))
				assert(stream:close())
			end)
		end

		gc()
		assert(system.run() == false)
		assert(complete == true)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = system.tcp("listen", domain)
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
			local server = system.tcp("listen", domain)
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
			local stream = system.tcp("stream", domain)
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
			local server = system.tcp("listen", domain)
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
			local server = assert(system.tcp("listen", domain))
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
			local stream = assert(system.tcp("stream", domain))
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
			local server = assert(system.tcp("listen", domain))
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
		local stream = system.tcp("stream", domain)

		assert(stream.listen == nil)
		assert(stream.accept == nil)

		done()
	end

	do case "successful connection"
		local connected
		spawn(function ()
			local server = system.tcp("listen", domain)
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			connected = false
			assert(server:accept())
			connected = true
		end)
		assert(connected == false)

		local stage = 0
		spawn(function ()
			local stream = system.tcp("stream", domain)
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
			local stream = system.tcp("stream", domain)
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
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			local stream = system.tcp("stream", domain)
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
		local server = system.tcp("listen", domain)
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local stream = system.tcp("stream", domain)
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

end
