local system = require "coutil.system"

newtest "open" -----------------------------------------------------------------

local validpath = "/dev/null"
local validmodes = "rwanNrstwx"

do case "non existent file"
	spawn(function ()
		asserterr("no such file or directory", system.file("./non/existent/file/path"))
	end)
	system.run()

	done()
end

do case "invalid modes"
	spawn(function ()
		for i = 1, 255 do
			local char = string.char(i)
			if not string.find(validmodes, char, 1, true) then
				asserterr("unknown mode char", pcall(system.file, validpath, char))
			end
		end
	end)
	system.run()

	done()
end

do case "ignore invalid permission"
	spawn(function ()
		assert(system.file(validpath, "w", "invalid")):close()
		assert(system.file(validpath, "w", "rw", "invalid")):close()
		assert(system.file(validpath, "w", "rw", "rw", "invalid")):close()
	end)
	system.run()

	done()
end

do case "invalid permission"
	spawn(function ()
		for i = 1, 255 do
			local char = string.char(i)
			if char ~= "r" and char ~= "w" and char ~= "x" then
				asserterr("unknown perm char", pcall(system.file, "./non-existent.txt", "N", char))
			end
		end
	end)
	system.run()

	done()
end
