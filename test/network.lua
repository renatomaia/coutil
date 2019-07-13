local system = require "coutil.system"

newtest "findaddr"

local hosts = {
	localhost = { ["127.0.0.1"] = "ipv4" },
	["ip6-localhost"] = { ["::1"] = "ipv6" },
	["*"] = {
		["0.0.0.0"] = "ipv4",
		["::"] = "ipv6",
	},
	[false] = {
		["127.0.0.1"] = "ipv4",
		["::1"] = "ipv6",
	},
}
local servs = {
	ssh = 22,
	http = 80,
	[false] = 0,
	[54321] = 54321,
}
local scktypes = {
	datagram = true,
	stream = true,
	listen = true,
}
local addrtypes = {
	ipv4 = system.address("ipv4"),
	ipv6 = system.address("ipv6"),
}

local function allexpected(ips, hostname, servport)
	local stream = hostname == "*" and "listen" or "stream"
	local missing = {}
	for literal, domain in pairs(ips) do
		missing[literal.." "..servport.." datagram"] = true
		missing[literal.." "..servport.." "..stream] = true
	end
	return missing
end

local function clearexpected(missing, found, scktype)
	missing[found.literal.." "..found.port.." "..scktype] = nil
end

do case "reusing addresses"
	spawn(function ()
		for hostname, ips in pairs(hosts) do
			for servname, servport in pairs(servs) do
				if servname or (hostname and hostname ~= "*") then
					local missing = allexpected(ips, hostname, servport)
					local getnext, last = assert(system.findaddr(hostname or nil, servname or nil))
					repeat
						local addr = assert(addrtypes[last])
						local other = last == "ipv4" and addrtypes.ipv6 or addrtypes.ipv4
						asserterr("wrong domain", pcall(getnext, other))
						local found, scktype, domain = getnext(addr)
						assert(rawequal(found, addr))
						assert(ips[found.literal] == last)
						assert(found.port == servport)
						assert(scktypes[scktype] == true)
						assert(domain == nil or addrtypes[domain] ~= nil)
						clearexpected(missing, found, scktype)
						last = domain
					until last == nil
					assert(_G.next(missing) == nil)
				end
			end
		end
	end)
	assert(system.run() == false)
	done()
end

do case "create addresses"
	spawn(function ()
		for hostname, ips in pairs(hosts) do
			for servname, servport in pairs(servs) do
				if servname or (hostname and hostname ~= "*") then
					local missing = allexpected(ips, hostname, servport)
					local getnext = assert(system.findaddr(hostname or nil, servname or nil))
					repeat
						local found, scktype, domain = assert(getnext())
						assert(ips[found.literal] ~= nil)
						assert(found.port == servport)
						assert(scktypes[scktype] == true)
						clearexpected(missing, found, scktype)
					until domain == nil
					assert(_G.next(missing) == nil)
				end
			end
		end
	end)
	assert(system.run() == false)
	done()
end

do case "for iteration"
	spawn(function ()
		for hostname, ips in pairs(hosts) do
			for servname, servport in pairs(servs) do
				if servname or (hostname and hostname ~= "*") then
					local missing = allexpected(ips, hostname, servport)
					for resaddr, scktype, domain in system.findaddr(hostname or nil, servname or nil) do
						assert(ips[resaddr.literal] ~= nil)
						assert(resaddr.port == servport)
						assert(scktypes[scktype] == true)
						clearexpected(missing, resaddr, scktype)
					end
					assert(_G.next(missing) == nil)
				end
			end
		end
	end)
	assert(system.run() == false)
	done()
end

do case "only first"
	spawn(function ()
		for hostname, ips in pairs(hosts) do
			for servname, servport in pairs(servs) do
				if servname or (hostname and hostname ~= "*") then
					local resaddr, scktype, domain = assert(system.findaddr(hostname or nil, servname or nil))()
					assert(ips[resaddr.literal] ~= nil)
					assert(resaddr.port == servport)
					assert(scktypes[scktype] == true)
				end
			end
		end
	end)
	assert(system.run() == false)
	done()
end

do case "error messages"
	spawn(function ()
		asserterr("name or service must be provided", pcall(system.findaddr))
		asserterr("name or service must be provided", pcall(system.findaddr, nil))
		asserterr("name or service must be provided", pcall(system.findaddr, nil, nil))

		asserterr("service must be provided for '*'", pcall(system.findaddr, "*"))
		asserterr("service must be provided for '*'", pcall(system.findaddr, "*"))
		asserterr("service must be provided for '*'", pcall(system.findaddr, "*", nil))

		asserterr("'m' is invalid for IPv4", pcall(system.findaddr, "localhost", 0, "m4"))
		asserterr("unknown mode char (got 'i')", pcall(system.findaddr, "localhost", 0, "ipv6"))
	end)

	assert(system.run() == false)

	done()
end

newtest "nameaddr"

local hosts = {
	localhost = { ipv4 = "127.0.0.1" },
	["ip6-localhost"] = { ipv6 = "::1" },
}
local servs = {
	ssh = 22,
	http = 80,
}
local names = {
	["www.tecgraf.puc-rio.br"] = "webserver.tecgraf.puc-rio.br",
}

do case "host and service"
	spawn(function ()
		for servname, port in pairs(servs) do
			assert(system.nameaddr(port) == servname)
			for hostname, ips in pairs(hosts) do
				for domain, ip in pairs(ips) do
					local addr = system.address(domain, ip, port)
					local name, service = system.nameaddr(addr)
					assert(name == hostname)
					assert(service == servname)
				end
			end
		end
	end)

	assert(system.run() == false)
	done()
end

do case "cannonical name"
	spawn(function ()
		for name, cannonical in pairs(names) do
			assert(system.nameaddr(name) == cannonical)
		end
	end)

	assert(system.run() == false)
	done()
end

newtest "socket"

do case "error messages"
	for value in pairs(types) do
		asserterr("bad argument", pcall(system.socket, "datagram"))
		asserterr("bad argument", pcall(system.socket, value, "ipv4"))
		asserterr("bad argument", pcall(system.socket, "stream", value))
		asserterr("bad argument", pcall(system.socket, "listen", value))
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
			system.suspend()
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
			system.suspend()
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
			system.suspend()
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
			system.suspend()
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
			system.suspend()
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

	newtest "stream:receive"

	local memory = require "memory"

	local backlog = 3

	do case "errors"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local buffer = memory.create(64)

		local stage1 = 0
		local accepted, stream
		spawn(function ()
			stage1 = 1
			accepted = assert(server:accept())
			stage1 = 2
			asserterr("memory expected", pcall(accepted.receive, accepted))
			asserterr("number has no integer representation",
				pcall(accepted.receive, accepted, buffer, 1.1))
			asserterr("number has no integer representation",
				pcall(accepted.receive, accepted, buffer, nil, 2.2))
			stage1 = 3
			assert(accepted:receive(buffer) == 64)
			stage1 = 4
			assert(stream:close() == true)
			stage1 = 5
			asserterr("operation canceled", accepted:send(string.rep("x", 1<<24)))
			stage1 = 6
		end)
		assert(stage1 == 1)

		local stage2 = 0
		spawn(function ()
			stream = assert(system.socket("stream", domain))
			stage2 = 1
			assert(stream:connect(ipaddr[domain].localaddress))
			stage2 = 2
			asserterr("already in use", pcall(accepted.receive, accepted, buffer))
			stage2 = 3
			assert(stream:send(string.rep("x", 64)) == true)
			stage2 = 4
			asserterr("closed", stream:receive(buffer))
			stage2 = 5
			system.suspend()
			assert(accepted:close() == true)
			stage2 = 6
		end)
		assert(stage2 == 1)

		assert(system.run("step") == true)
		assert(stage1 == 2)
		assert(stage2 == 3)

		asserterr("unable to yield", pcall(accepted.receive, accepted, buffer))

		gc()
		assert(system.run() == false)
		assert(stage1 == 6)
		assert(stage2 == 6)

		assert(server:close() == true)

		done()
	end

	do case "successful transmissions"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local data = string.rep("a", 64)
		           ..string.rep("b", 32)
		           ..string.rep("c", 32)

		local function assertfilled(buffer, count)
			local expected = string.sub(data, 1, count)..string.rep("\0", #data-count)
			assert(memory.diff(buffer, expected) == nil)
		end

		local done1
		spawn(function ()
			local stream = assert(server:accept())
			assert(server:close())
			local buffer = memory.create(128)
			assert(stream:receive(buffer) == 64)
			assertfilled(buffer, 64)
			assert(stream:receive(buffer, 65, 96) == 32)
			assertfilled(buffer, 96)
			assert(stream:receive(buffer, 97) == 32)
			assertfilled(buffer, 128)
			done1 = true
		end)
		assert(done1 == nil)

		local done2
		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			assert(stream:send(data, 1, 64))
			system.suspend()
			assert(stream:send(data, 65, 128))
			--assert(stream:close())
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
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local size = 32
		local buffer = memory.create(size)

		local stream, thread
		spawn(function ()
			stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
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
				local server = assert(system.socket("listen", domain))
				assert(server:bind(ipaddr[domain][addrname]))
				assert(server:listen(backlog))

				local stream = assert(server:accept())
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
				local stream = assert(system.socket("stream", domain))
				assert(stream:connect(ipaddr[domain][addrname]))
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			local buffer = memory.create("9876543210")
			assert(stream:receive(buffer) == 10)
			assert(memory.diff(buffer, "0123456789") == nil)
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:receive(memory.create(10)) == garbage)
			stage = 2
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
			coroutine.resume(garbage.coro, garbage)
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	newtest "stream:send"

	do case "errors"
		local server = assert(system.socket("listen", domain))
		assert(server:bind(ipaddr[domain].localaddress))
		assert(server:listen(backlog))

		local data = string.rep("x", 1<<24)

		local stage1 = 0
		local accepted, stream
		spawn(function ()
			stage1 = 1
			accepted = assert(server:accept())
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
			stream = assert(system.socket("stream", domain))
			stage2 = 1
			assert(stream:connect(ipaddr[domain].localaddress))
			stage2 = 2
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
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
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:send("0123456789") == true)
			stage = 2
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
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
			local server = assert(system.socket("listen", domain))
			assert(server:bind(ipaddr[domain].localaddress))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:send(string.rep("x", 1<<24)) == garbage)
			stage = 2
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(system.socket("stream", domain))
			assert(stream:connect(ipaddr[domain].localaddress))
			system.suspend()
			coroutine.resume(garbage.coro, garbage)

			assert(stream:close())
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	newtest "datagram:receive"

	local memory = require "memory"

	local backlog = 3

	do case "errors"
		local buffer = memory.create(64)

		local connected, unconnected
		local stage1 = 0
		spawn(function ()
			stage1 = 1
			unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
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
			connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			assert(memory.diff(buffer, expected) == nil)
		end

		local done1
		spawn(function ()
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(128)
			assert(unconnected:receive(buffer) == 64)
			assertfilled(buffer, 64)
			assert(unconnected:receive(buffer, 65, 96) == 32)
			assertfilled(buffer, 96)
			assert(unconnected:receive(buffer, 97) == 0)
			assertfilled(buffer, 96)
			done1 = true
		end)
		assert(done1 == nil)

		local done2
		spawn(function ()
			local connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			assert(unconnected:receive(buffer) == size)
			assert(memory.diff(buffer, string.rep("a", size)) == nil)
			coroutine.resume(thread)
		end)

		local complete = false
		spawn(function ()
			thread = coroutine.running()
			coroutine.yield()
			asserterr("already in use", pcall(unconnected.receive, unconnected))
			coroutine.yield()
			assert(unconnected:receive(buffer) == size)
			assert(memory.diff(buffer, string.rep("b", size)) == nil)
			assert(unconnected:close())
			complete = true
		end)

		spawn(function ()
			local connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
		for i, addrname in ipairs{"localaddress", "freeaddress"} do
			local connected, unconnected
			spawn(function ()
				unconnected = assert(system.socket("datagram", domain))
				assert(unconnected:bind(ipaddr[domain][addrname]))
				spawn(function ()
					local buffer = memory.create(64)
					assert(unconnected:receive(buffer) == 32)
					memory.fill(buffer, buffer, 33, 64)
					assert(unconnected:send(buffer, nil, nil, connected:getaddress()))
				end)
			end)

			spawn(function ()
				connected = assert(system.socket("datagram", domain))
				assert(connected:connect(ipaddr[domain][addrname]))
				assert(connected:send(string.rep("x", 32)))
				local buffer = memory.create(64)
				local addr = system.address(domain)
				assert(connected:receive(buffer, nil, nil, addr) == 64)
				assert(memory.diff(buffer, string.rep("x", 64)) == nil)
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create("9876543210")
			local bytes, extra = unconnected:receive(buffer)
			assert(bytes == nil)
			assert(extra == nil)
			assert(memory.diff(buffer, "9876543210") == nil)
			stage = 2
			assert(unconnected:receive(buffer) == 10)
			assert(memory.diff(buffer, "0123456789") == nil)
			assert(unconnected:close())
			stage = 3
		end)
		assert(stage == 1)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			local connected = assert(system.socket("datagram", domain))
			assert(connected:send("0123456789", nil, nil, ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create("9876543210")
			stage = 1
			assert(unconnected:receive(buffer) == 10)
			assert(memory.diff(buffer, "0123456789") == nil)
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:send("0123456789", nil, nil, ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
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

	newtest "datagram:send"

	do case "errors"
		local data = string.rep("x", 1<<24)
		local addr = system.address(domain)

		local complete, connected
		spawn(function ()
			connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			unconnected = assert(system.socket("datagram", domain))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(128)
			assert(unconnected:receive(buffer) == 128)
			assert(memory.diff(buffer, string.rep("x", 128)) == nil)
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(system.socket("datagram", domain))
			stage2 = 1
			local res, a,b,c = unconnected:send(string.rep("x", 128), nil, nil,
			                                    ipaddr[domain].localaddress)
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(128)
			stage1 = 2
			assert(unconnected:receive(buffer) == 128)
			assert(memory.diff(buffer, string.rep("x", 128)) == nil)
			local buffer = memory.create(64)
			assert(unconnected:receive(buffer) == 64)
			assert(memory.diff(buffer, string.rep("x", 64)) == nil)
			stage1 = 3
		end)
		assert(stage1 == 2)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			stage2 = 1
			local connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(128)
			stage1 = 2
			assert(unconnected:receive(buffer) == 128)
			assert(memory.diff(buffer, string.rep("x", 128)) == nil)
			--local buffer = memory.create(64)
			--assert(unconnected:receive(buffer) == 64)
			--assert(memory.diff(buffer, string.rep("x", 64)) == nil)
			stage1 = 3
		end)
		assert(stage1 == 2)

		local stage2 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			stage2 = 1
			local connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(10)
			stage1 = 1
			assert(unconnected:receive(buffer) == 10)
			assert(memory.diff(buffer, "0123456789") == nil)
			stage1 = 2
		end)
		assert(stage1 == 1)

		local stage2 = 0
		pspawn(function ()
			local connected = assert(system.socket("datagram", domain))
			assert(connected:connect(ipaddr[domain].localaddress))
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
			local unconnected = assert(system.socket("datagram", domain))
			assert(unconnected:bind(ipaddr[domain].localaddress))
			local buffer = memory.create(128)
			assert(unconnected:receive(buffer) == 128)
			assert(memory.diff(buffer, string.rep("x", 128)) == nil)
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local unconnected = assert(system.socket("datagram", domain))
			stage2 = 1
			local res, a,b,c = unconnected:send(string.rep("x", 128), nil, nil,
			                                    ipaddr[domain].localaddress)
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

end