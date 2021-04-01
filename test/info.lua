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
local cpuinfo<close> = system.cpuinfo()

assert(#cpuinfo > 0)

do case "errors"
	for cpuidx = 1, #cpuinfo do
		for i = 1, 255 do
			local char = string.char(i)
			if not string.find(options, char, 1, "plain search") then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(cpuinfo, char, cpuidx-1))
			end
		end
	end

	done()
end

do case "single value"
	for cpuidx = 1, #cpuinfo do
		for c in string.gmatch(options , ".") do
			local ltype = (c == "m") and "string" or "number"
			local i, value = cpuinfo(c, cpuidx-1)
			assert(i == cpuidx)
			assert(type(value) == ltype)

			local i, v1, v2, v3 = cpuinfo(c..c..c, cpuidx-1)
			assert(i == cpuidx)
			assert(type(v1) == ltype)
			assert(v2 == v1)
			assert(v3 == v1)
		end
	end

	done()
end

do case "all values"
	for cpuidx = 1, #cpuinfo do
		local packed = table.pack(cpuinfo(options, cpuidx-1))
		assert(#packed == 1+#options)
		assert(packed[1] == cpuidx)
		for i = 1, #options do
			local ltype = (options:sub(i, i) == "m") and "string" or "number"
			assert(type(packed[i+1]) == ltype)
		end
	end

	done()
end

do case "close"
	do local tobeclosed<close> = cpuinfo end
	asserterr("closed", pcall(cpuinfo, "m"))
	asserterr("closed", pcall(function () return #cpuinfo end))

	done()
end

newtest "netinfo"

local options = "nidbBtTlm"
local netinfo = assert(system.netinfo("all"))

do case "errors"
	asserterr("invalid option", pcall(system.netinfo, "list"))
	asserterr("number expected", pcall(system.netinfo, "name"))
	asserterr("number expected", pcall(system.netinfo, "id"))
	asserterr("out of range", pcall(system.netinfo, "id", -1))
	asserterr("out of range", pcall(system.netinfo, "id", 0))
	asserterr("out of range", pcall(system.netinfo, "id", math.maxinteger))

	for i = 1, 255 do
		local char = string.char(i)
		if not string.find(options, char, 1, "plain search") then
			asserterr("unknown mode char (got '"..char.."')",
			          pcall(netinfo, char, 0))
		end
	end

	assert(netinfo("indt", -2) == nil)
	assert(netinfo("indt", -1) == nil)
	assert(netinfo("indt", #netinfo) == nil)
	assert(netinfo("indt", #netinfo+1) == nil)

	done()
end

do case "values"
	for i = 1, math.huge do
		local result = system.netinfo("name", i)
		if not result then break end
		assert(type(result) == "string")
		assert(type(system.netinfo("id", i)) == "string")
	end

	assert(#netinfo >= 0)

	local typeof = {
		n = "string",
		i = "boolean",
		d = "string",
		b = "string",
		B = "string",
		t = "string",
		T = "string",
		l = "number",
		m = "string",
	}
	for i = 0, #netinfo-1 do
		for c in string.gmatch(options , ".") do
			local index, value = netinfo(c, i)
			assert(index == i+1)

			local ltype = type(value)
			assert(ltype == typeof[c])

			local index, v1, v2, v3 = netinfo(c..c..c, i)
			assert(index == i+1)

			assert(type(v1) == ltype)
			assert(v2 == v1)
			assert(v3 == v1)
		end

		local _, domain = netinfo("d", i)
		assert(domain:match("^ipv[46]$"))

		local _, masklen, literal, binary = netinfo("ltb", i)
		assert(masklen <= 8*#binary)

		local addressT = system.address(domain, literal, 65432, "t")
		local addressB = system.address(domain, binary, 65432, "b")
		assert(addressT == addressB)

		local _, macliteral = netinfo("T", i)
		local _, macbinary = netinfo("B", i)
		assert(#macbinary == 6)
		assert(macliteral == string.format("%02x:%02x:%02x:%02x:%02x:%02x",
		                                   string.byte(macbinary, 1, 6)))
	end

	done()
end

do case "close"
	do local tobeclosed<close> = netinfo end
	asserterr("closed", pcall(netinfo, "t", 0))
	asserterr("closed", pcall(function () return #netinfo end))

	done()
end
