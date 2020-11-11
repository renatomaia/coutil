local info = require "coutil.info"

local options = {
	getusage = "cmUSTMd=pPwio><sxX",
	getsystem = "fpu1lL",
}

for fname, options in pairs(options) do
	newtest(fname)

	local func = info[fname]

	do case "errors"
		for i = 1, 255 do
			local char = string.char(i)
			if not string.find(options, char, 1, "plain search") then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(func, char))
			end
		end

		done()
	end

	do case "single value"
		for c in string.gmatch(options , ".") do
			local value = func(c)
			assert(type(value) == "number")

			local v1, v2, v3 = func(c..c..c)
			assert(type(v1) == "number")
			assert(v2 == v1)
			assert(v3 == v1)
		end

		done()
	end

	do case "all values"
		local packed = table.pack(func(options))
		assert(#packed == #options)
		for i = 1, #options do
			assert(type(packed[i]) == "number")
		end

		done()
	end
end

newtest "getcpustat"

do case "errors"
	local cpuinfo = info.getcpustat()
	local count = cpuinfo:count()

	for _, opname in ipairs{
		"model",
		"speed",
		"usertime",
		"nicetime",
		"systemtime",
		"idletime",
		"irqtime",
	} do
		for _, i in ipairs{ -1, 0, count+1, math.maxinteger } do
			asserterr("index out of bound", pcall(cpuinfo[opname], cpuinfo, i))
		end
	end

	done()
end

do case "attributes"
	local cpuinfo = info.getcpustat()
	local count = cpuinfo:count()

	for i = 1, count do
		for opname, expected in pairs{
			model = "string",
			speed = "number",
			usertime = "number",
			nicetime = "number",
			systemtime = "number",
			idletime = "number",
			irqtime = "number",
		} do
			local value = cpuinfo[opname](cpuinfo, i)
			assert(expected == type(value))
		end
	end

	done()
end
