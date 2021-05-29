local memory = require "memory"
local system = require "coutil.system"

newtest("stdio") --------------------------------------------------------------

do case "file"
	local inputpath = os.tmpname()
	local outputpath = os.tmpname()
	writeto(inputpath, "Hello world!\n")
	local stdin = assert(io.open(inputpath, "r"))
	local stdout = assert(io.open(outputpath, "w"))
	spawn(function ()
		local res, val = system.execute{
			execfile = luabin,
			stdin = stdin,
			stdout = stdout,
			arguments = { "-e", utilschunk..[[
				local memory = require "memory"
				local system = require "coutil.system"
				spawn(function ()
					local buffer = memory.create(1024)
					local bytes = system.stdin:read(buffer)
					if standard == "win32" then
						assert(bytes == 14)
						assert(buffer:tostring(1, 14) == "Hello world!\r\n")
					else
						assert(bytes == 13)
						assert(buffer:tostring(1, 13) == "Hello world!\n")
					end
					system.stdout:write("Goodbye world!\n")
				end)
				system.run()
			]] }
		}
		assert(res == "exit")
		assert(val == 0)

		assert(readfrom(outputpath) == "Goodbye world!\n")
	end)
	assert(stdin:close())
	assert(stdout:close())

	system.run()

	done()
end

do case "pipe"
	spawn(function ()
		local procinfo = {
			execfile = luabin,
			stdin = "w",
			stdout = "r",
			arguments = { "-e", utilschunk..[[
				local memory = require "memory"
				local system = require "coutil.system"
				spawn(function ()
					local buffer = memory.create(1024)
					local bytes = system.stdin:read(buffer)
					assert(bytes == 13)
					assert(buffer:tostring(1, 13) == "Hello world!\n")
					system.stdout:write("Goodbye world!\n")
				end)
				system.run()
			]] }
		}
		spawn(system.execute, procinfo)
		assert(procinfo.stdin:write("Hello world!\n"))
		local buffer = memory.create(1024)
		local bytes = procinfo.stdout:read(buffer)
		assert(bytes == 15)
		assert(buffer:tostring(1, 15) == "Goodbye world!\n")
	end)

	system.run()

	done()
end
