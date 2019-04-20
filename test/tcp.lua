local system = require "coutil.system"

newtest "tcpsock" --------------------------------------------------------------

do case "error messages"
	asserterr("invalid option", pcall(system.tcp, "datagram"))
	asserterr("invalid option", pcall(system.tcp, nil, "file"))
	asserterr("invalid option", pcall(system.tcp, nil, "unix"))

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
	ipaddr[domain].remotetcp = system.address(domain, ipaddr[domain].dnshost1, 53)
	ipaddr[domain].othertcp = system.address(domain, ipaddr[domain].dnshost2, 53)
end

for _, domain in ipairs{"ipv4", "ipv6"} do
	for _, kind in ipairs{"stream", "listen"} do

		do case(kind.." "..domain.." garbage generation")
			garbage.tcp = system.tcp(kind, domain)

			done()
		end

		do case(kind.." "..domain.." garbage collection")
			garbage.tcp = system.tcp(kind, domain)

			gc()
			assert(garbage.tcp == nil)

			assert(system.run() == false)

			done()
		end

		do case(kind.." "..domain.." close")
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

		do case(kind.." "..domain.." getdomain")
			local tcp = system.tcp(kind, domain)
			assert(tcp:getdomain() == domain)

			done()
		end

		do case(kind.." "..domain.." address")
			local tcp = system.tcp(kind, domain)

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
end

newtest "tcpstream" ------------------------------------------------------------

for _, domain in ipairs{"ipv4", "ipv6"} do
	while false do case("options ("..domain..")")
		local stream = system.tcp("stream")

		asserterr("string expected, got no value", pcall(stream.getoption, stream))
		asserterr("string expected, got no value", pcall(stream.setoption, stream))
		asserterr("value expected", pcall(stream.setoption, stream, "keepalive"))
		asserterr("invalid option", pcall(stream.getoption, stream, "KeepAlive"))
		asserterr("invalid option", pcall(stream.setoption, stream, "KeepAlive", 1))
		asserterr("invalid argument", stream:setoption("keepalive", 0))
		asserterr("number has no integer representation",
			pcall(stream.setoption, stream, "keepalive", 0.123))

		assert(stream:getoption("keepalive") == nil)
		assert(stream:setoption("keepalive", 123) == true)
		assert(stream:getoption("keepalive") == 123)
		assert(stream:setoption("keepalive", false) == true)
		assert(stream:getoption("keepalive") == nil)

		assert(stream:getoption("nodelay") == false)
		assert(stream:setoption("nodelay", true) == true)
		assert(stream:getoption("nodelay") == true)
		assert(stream:setoption("nodelay", false) == true)

		done()
	end
end

do case "scheduled connect/accept"
	for _, domain in ipairs{ "ipv4", "ipv6" } do
		local connected
		spawn(function ()
			local server = system.tcp("listen", domain)
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen())
			connected = false
			local stream = assert(server:accept())
			assert(stream:close())
			--assert(server:close())
			connected = true
		end)
		assert(connected == false)

		local stage = 0
		spawn(function ()
			local stream = system.tcp("stream", domain)
			assert(stream:connect(ipaddr[domain].localaddress))
			assert(stream:close())
			stage = 1
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
		assert(connected == true)
	end

	done()
end

--[=====================================[
do case "scheduled yield"
	local stage = 0
	spawn(function ()
		awaitsig("user1")
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule same signal"
	local stage = 0
	spawn(function ()
		awaitsig("user1")
		stage = 1
		awaitsig("user1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		sendsignal("USR1")
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run("step") == true)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "reschedule different signal"
	local stage = 0
	spawn(function ()
		awaitsig("user1")
		stage = 1
		awaitsig("user2")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		sendsignal("USR1")
		pause()
		sendsignal("USR2")
	end)

	gc()
	assert(run("step") == true)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local ok, a,b,c = awaitsig("user1")
		assert(ok == nil)
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
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "cancel and reschedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local ok, extra = awaitsig("user1")
		assert(ok == nil)
		assert(extra == nil)
		stage = 1
		assert(awaitsig("user1") == true)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause() -- the first signal handle is active.
		pause() -- the first signal handle is being closed.
		sendsignal("USR1") -- the second signal handle is active.
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "resume while closing"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		assert(awaitsig("user1") == nil)
		stage = 1
		local ok, a,b,c = awaitsig("user1")
		assert(ok == nil)
		assert(a == .1)
		assert(b == 2.2)
		assert(c == 33.3)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
		assert(stage == 2)
		stage = 3
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 3)

	done()
end

do case "ignore errors"

	local stage = 0
	pspawn(function ()
		assert(awaitsig("user1"))
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		awaitsig("user1")
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

--]=====================================]

