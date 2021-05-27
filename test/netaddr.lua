local system = require "coutil.system"

newtest "address" --------------------------------------------------------------

do case "empty ipv4"
	local a = system.address("ipv4")

	assert(tostring(a) == "0.0.0.0:0")
	assert(a.type == "ipv4")
	assert(a.port == 0)
	assert(a.literal == "0.0.0.0")
	assert(a.binary == "\0\0\0\0")
	assert(a == system.address("ipv4"))

	done()
end

do case "empty ipv6"
	local a = system.address("ipv6")

	assert(tostring(a) == "[::]:0")
	assert(a.type == "ipv6")
	assert(a.port == 0)
	assert(a.literal == "::")
	assert(a.binary == string.rep("\0", 16))
	assert(a == system.address("ipv6"))

	done()
end

local cases = {
	ipv4 = {
		port = 8080,
		literal = "192.168.0.1",
		binary = "\192\168\000\001",
		uri = "192.168.0.1:8080",
		changes = {
			port = 54321,
			literal = "127.0.0.1",
			binary = "byte",
		},
		equivalents = {
			["10.20.30.40"] = "\10\20\30\40",
			["40.30.20.10"] = "\40\30\20\10",
		},
		invalid = "291.168.0.1",
	},
	ipv6 = {
		port = 8888,
		literal = "::ffff:192.168.0.1",
		binary = "\0\0\0\0\0\0\0\0\0\0\xff\xff\192\168\000\001",
		uri = "[::ffff:192.168.0.1]:8888",
		changes = {
			port = 12345,
			literal = "::1",
			binary = "bytebytebytebyte",
		},
		equivalents = {
			["1::f"] =
				"\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\x0f",
			["1:203:405:607:809:a0b:c0d:e0f"] =
				"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
			["123:4567:89ab:cdef:fedc:ba98:7654:3210"] =
				"\x01\x23\x45\x67\x89\xab\xcd\xef\xfe\xdc\xba\x98\x76\x54\x32\x10",
		},
		invalid = "[291:168:0:1]",
	},
}
for type, expected in pairs(cases) do
	local addr = system.address(type, expected.uri)

	local function checkaddr(a)
		--assert(system.type(a) == "address")
		assert(tostring(a) == expected.uri)
		assert(a.type == type)
		assert(a.port == expected.port)
		assert(a.literal == expected.literal)
		assert(a.binary == expected.binary)
		assert(a == addr)
	end

	do case("create "..type)
		checkaddr(addr)
		checkaddr(system.address(type, expected.literal, expected.port))
		checkaddr(system.address(type, expected.literal, expected.port, "t"))
		checkaddr(system.address(type, expected.binary, expected.port, "b"))

		done()
	end

	do case("change "..type)
		for field, newval in pairs(expected.changes) do
			local oldval = addr[field]
			addr[field] = newval
			assert(addr[field] == newval)
			addr[field] = oldval
			checkaddr(addr)
		end

		for literal, binary in pairs(expected.equivalents) do
			addr.literal = literal
			assert(addr.binary == binary)
		end
		for literal, binary in pairs(expected.equivalents) do
			addr.binary = binary
			assert(addr.literal == literal)
		end

		done()
	end

	do case("bad field "..type)
		local a = system.address(type)
		asserterr("bad argument #2 to 'index' (invalid option 'wrongfield')",
			pcall(function () return a.wrongfield end))
		asserterr("bad argument #2 to 'newindex' (invalid option 'wrongfield')",
			pcall(function () a.wrongfield = true end))
		asserterr("bad argument #2 to 'newindex' (invalid option 'type')",
			pcall(function () a.type = true end))
		if type == "file" then
			asserterr("bad argument #2 to 'newindex' (invalid option 'port')",
				pcall(function () a.port = 1234 end))
		end

		done()
	end

	do case("errors "..type)
		asserterr("bad argument #1 to 'coutil.system.address' (invalid option 'ip')",
			pcall(system.address, "ip"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
			pcall(system.address, type, true))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
			pcall(system.address, type, true, expected.port))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
			pcall(system.address, type, true, expected.port, "t"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
			pcall(system.address, type, true, expected.port, "b"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, nil))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, nil, nil))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, expected.port))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, expected.port, "t"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, expected.port, "b"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, nil))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, nil, "t"))
		asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
			pcall(system.address, type, nil, nil, "b"))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
			pcall(system.address, type, expected.uri, nil))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
			pcall(system.address, type, expected.literal, nil, "t"))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
			pcall(system.address, type, expected.binary, nil, "b"))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
			pcall(system.address, type, expected.literal, "port"))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
			pcall(system.address, type, expected.literal, "port", "t"))
		asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
			pcall(system.address, type, expected.literal, "port", "b"))
		asserterr("bad argument #4 to 'coutil.system.address' (invalid mode)",
			pcall(system.address, type, 3232235776, expected.port, "n"))

		asserterr("bad argument #2 to 'coutil.system.address' (invalid URI format)",
			pcall(system.address, type, expected.literal))
		asserterr("invalid argument",
			pcall(system.address, type, type == "ipv4" and "localhost:8080" or  "[ip6-localhost]:8080"))
		asserterr("invalid argument",
			pcall(system.address, type, expected.invalid..":8080"))
		asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, (string.gsub(expected.uri, expected.port.."$", "65536"))))
		asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, (string.gsub(expected.uri, expected.port.."$", "-"..expected.port))))
		asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, (string.gsub(expected.uri, expected.port.."$", "0x1f90"))))

		asserterr("invalid argument",
			pcall(system.address, type, "localhost", expected.port, "t"))
		asserterr("invalid argument",
			pcall(system.address, type, expected.invalid, expected.port, "t"))

		asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, expected.literal, 65536, "t"))
		asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, expected.literal, -1, "t"))
		asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, expected.binary, 65536, "b"))
		asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
			pcall(system.address, type, expected.binary, -1, "b"))

		done()
	end

end

newtest "findaddr" -------------------------------------------------------------

local hosts = {
	localhost = {
		["127.0.0.1"] = "ipv4",
		["::1"] = "ipv6",
	},
	--["ip6-localhost"] = { ["::1"] = "ipv6" },
	["*"] = {
		["0.0.0.0"] = "ipv4",
		["::"] = "ipv6",
	},
}
hosts[false] = hosts.localhost
local servs = {
	ssh = 22,
	http = 80,
	[false] = 0,
	[54321] = 54321,
}
local scktypes = {
	datagram = true,
	stream = true,
	passive = true,
}
local addrtypes = {
	ipv4 = system.address("ipv4"),
	ipv6 = system.address("ipv6"),
}

local function allexpected(ips, hostname, servport)
	local stream = hostname == "*" and "passive" or "stream"
	local missing = {}
	for literal, domain in pairs(ips) do
		missing[literal.." "..servport.." datagram"] = true
		missing[literal.." "..servport.." "..stream] = true
	end
	return missing
end

local function clearexpected(missing, found, scktype)
	if standard == "win32" and scktype == nil then
		missing[found.literal.." "..found.port.." datagram"] = nil
		missing[found.literal.." "..found.port.." stream"] = nil
		missing[found.literal.." "..found.port.." passive"] = nil
	else
		missing[found.literal.." "..found.port.." "..scktype] = nil
	end
end

do case "reusing addresses"
	spawn(function ()
		for hostname, ips in pairs(hosts) do
			for servname, servport in pairs(servs) do
				if servname or (hostname and hostname ~= "*") then
					local addrlist = assert(system.findaddr(hostname or nil, servname or nil))
					for i = 1, 2 do
						local missing = allexpected(ips, hostname, servport)
						local addr, domain, scktype
						repeat
							domain = addrlist:getdomain()
							addr = assert(addrtypes[domain])
							local other = domain == "ipv4" and addrtypes.ipv6 or addrtypes.ipv4
							asserterr("wrong domain", pcall(addrlist.getaddress, addrlist, other))
							local found = addrlist:getaddress(addr)
							assert(rawequal(found, addr))
							assert(ips[found.literal] == domain)
							assert(found.port == servport)
							scktype = addrlist:getsocktype()
							assert(domain == nil or addrtypes[domain] ~= nil)
							assert(standard == "win32" and scktype == nil or scktypes[scktype] == true)
							clearexpected(missing, found, scktype)
						until not addrlist:next()
						assert(domain == addrlist:getdomain())
						assert(addr == addrlist:getaddress())
						assert(scktype == addrlist:getsocktype())
						addrlist:reset()
					end
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
					local addrlist = assert(system.findaddr(hostname or nil, servname or nil))
					repeat
						local found = addrlist:getaddress()
						local scktype = addrlist:getsocktype()
						assert(addrlist:getdomain() == found.type)
						assert(ips[found.literal] ~= nil)
						assert(found.port == servport)
						assert(standard == "win32" and scktype == nil or scktypes[scktype] == true)
						clearexpected(missing, found, scktype)
					until not addrlist:next()
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

newtest "nameaddr" -------------------------------------------------------------

local hosts = {
	localhost = {
		ipv4 = "127.0.0.1",
		ipv6 = "::1",
	},
}
local servs = {
	ssh = 22,
	http = 80,
}
local names = {
	["www.github.com"] = "github.com",
}

do case "host and service"
	spawn(function ()
		for servname, port in pairs(servs) do
			assert(system.nameaddr(port) == servname)
			for hostname, ips in pairs(hosts) do
				for domain, ip in pairs(ips) do
					local addr = system.address(domain, ip, port)
					local name, service = system.nameaddr(addr)
					assert(name == standard == "win32" and system.procinfo("n") or hostname)
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
