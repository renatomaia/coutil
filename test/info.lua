local system = require "coutil.system"

local options = "#$^=<>1bcdefghHiklLmMnopPrRsStTuUvVwxX"

newtest("info")

do case "errors"
	for i = 1, 255 do
		local char = string.char(i)
		if not string.find(options, char, 1, "plain search") then
			asserterr("unknown mode char (got '"..char.."')",
			          pcall(system.info, char))
		end
	end

	done()
end

do case "single value"
	for c in string.gmatch(options , ".") do
		local ltype = type(system.info(c))
		assert(ltype == "number" or ltype == "string")

		local v1, v2, v3 = system.info(c..c..c)

		assert(type(v1) == ltype)
		assert(v2 == v1)
		assert(v3 == v1)
	end

	done()
end

do case "all values"
	asserterr("unknown mode char (got '\255')",
	          pcall(system.info, options.."\255"))

	local packed = table.pack(system.info(options))
	assert(#packed == #options)
	for i = 1, #options do
		local ltype = type(packed[i])
		assert(ltype == "number" or ltype == "string")
	end

	done()
end

newtest "cpuinfo"

local options = "mcunsid"
local cpuinfo = system.cpuinfo()

assert(cpuinfo:count() > 0)

do case "errors"
	for cpuidx = 1, cpuinfo:count() do
		for i = 1, 255 do
			local char = string.char(i)
			if not string.find(options, char, 1, "plain search") then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(cpuinfo.stats, cpuinfo, cpuidx, char))
			end
		end
	end

	done()
end

do case "single value"
	for cpuidx = 1, cpuinfo:count() do
		for c in string.gmatch(options , ".") do
			local ltype = (c == "m") and "string" or "number"
			local value = cpuinfo:stats(cpuidx, c)
			assert(type(value) == ltype)

			local v1, v2, v3 = cpuinfo:stats(cpuidx, c..c..c)
			assert(type(v1) == ltype)
			assert(v2 == v1)
			assert(v3 == v1)
		end
	end

	done()
end

do case "all values"
	for cpuidx = 1, cpuinfo:count() do
		local packed = table.pack(cpuinfo:stats(cpuidx, options))
		assert(#packed == #options)
		for i = 1, #options do
			local ltype = (options:sub(i, i) == "m") and "string" or "number"
			assert(type(packed[i]) == ltype)
		end
	end

	done()
end

do case "close"
	assert(cpuinfo:close() == true)
	asserterr("closed", pcall(cpuinfo.close, cpuinfo))
	asserterr("closed", pcall(cpuinfo.count, cpuinfo))
	asserterr("closed", pcall(cpuinfo.stats, cpuinfo, 1))

	done()
end

newtest "netiface"

local netifaces = assert(system.netiface("all"))

do case "errors"
	asserterr("invalid option", pcall(system.netiface, "list"))
	asserterr("number expected", pcall(system.netiface, "name"))
	asserterr("number expected", pcall(system.netiface, "id"))
	asserterr("out of range", pcall(system.netiface, "id", -1))
	asserterr("out of range", pcall(system.netiface, "id", 0))
	asserterr("out of range", pcall(system.netiface, "id", math.maxinteger))

	for _, i in ipairs{ 0, netifaces:count()+1 } do
		asserterr("out of range", pcall(netifaces.isinternal, netifaces, i))
		asserterr("out of range", pcall(netifaces.getname, netifaces, i))
		asserterr("out of range", pcall(netifaces.getdomain, netifaces, i))
		asserterr("out of range", pcall(netifaces.getaddress, netifaces, i))
	end

	asserterr("invalid mode", pcall(netifaces.getaddress, netifaces, 1, "binary"))
	asserterr("invalid mode", pcall(netifaces.getaddress, netifaces, 1, "text"))
	asserterr("invalid mode", pcall(netifaces.getaddress, netifaces, 1, "address"))

	for _, e in ipairs(types) do
		if type(e) ~= "string" and type(e) ~= "number" then
			asserterr("string expected", pcall(netifaces.getaddress, netifaces, 1, e))
		end
	end

	done()
end

do case "values"
	for i = 1, math.huge do
		local result = system.netiface("name", i)
		if not result then break end
		assert(type(result) == "string")
		assert(type(system.netiface("id", i)) == "string")
	end

	assert(netifaces:count() >= 0)

	for i = 1, netifaces:count() do
		assert(type(netifaces:isinternal(i)) == "boolean")
		assert(type(netifaces:getname(i)) == "string")

		local domain = netifaces:getdomain(i)
		assert(domain:match("^ipv[46]$"))

		local literal, masklenT = netifaces:getaddress(i)
		assert(literal == netifaces:getaddress(i, "t"))

		local binary, masklenB = netifaces:getaddress(i, "b")
		assert(masklenB <= 8*#binary)

		local addressT = system.address(domain, literal, 65432, "t")
		local addressB = system.address(domain, binary, 65432, "b")
		assert(addressT == addressB)
		assert(masklenT == masklenB)

		local address = system.address(domain)
		address.port = 65432
		assert(rawequal(netifaces:getaddress(i, address), address))
		assert(address == addressT)

		local literal = netifaces:getmac(i)
		assert(literal == netifaces:getmac(i, "t"))

		local binary = netifaces:getmac(i, "b")
		assert(#binary == 6)
		assert(literal == string.format("%02x:%02x:%02x:%02x:%02x:%02x", string.byte(binary, 1, 6)))
	end

	done()
end

do case "close"
	assert(netifaces:close() == true)
	asserterr("closed", pcall(netifaces.close, netifaces))
	asserterr("closed", pcall(netifaces.count, netifaces))
	asserterr("closed", pcall(netifaces.isinternal, netifaces, 1))
	asserterr("closed", pcall(netifaces.getname, netifaces, 1))
	asserterr("closed", pcall(netifaces.getdomain, netifaces, 1))
	asserterr("closed", pcall(netifaces.getaddress, netifaces, 1))
	asserterr("closed", pcall(netifaces.getmac, netifaces, 1))

	done()
end
