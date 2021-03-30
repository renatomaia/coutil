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
