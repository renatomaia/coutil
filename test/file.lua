local system = require "coutil.system"

newtest "openfile" -----------------------------------------------------------------

local validpath = "/dev/null"
local validmodes = "rwanNrstwx"

do case "non existent file"
	spawn(function ()
		asserterr("no such file or directory", system.openfile("./non/existent/file/path"))
	end)
	system.run()

	done()
end

do case "invalid modes"
	spawn(function ()
		for i = 1, 255 do
			local char = string.char(i)
			if not string.find(validmodes, char, 1, true) then
				asserterr("unknown mode char", pcall(system.openfile, validpath, char))
			end
		end
	end)
	system.run()

	done()
end

do case "ignore invalid permission"
	spawn(function ()
		assert(system.openfile(validpath, "w", "invalid")):close()
		assert(system.openfile(validpath, "w", "rw", "invalid")):close()
		assert(system.openfile(validpath, "w", "rw", "rw", "invalid")):close()
	end)
	system.run()

	done()
end

do case "invalid permission"
	spawn(function ()
		local valid = "UGSrwxRWX421"
		for i = 1, 255 do
			local char = string.char(i)
			if not valid:find(char, 1, true) then
				asserterr("unknown perm char", pcall(system.openfile, "./non-existent.txt", "N", char))
			end
		end
	end)
	system.run()

	done()
end
