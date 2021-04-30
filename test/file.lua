local memory = require "memory"
local system = require "coutil.system"

local buffer = memory.create(20)
local permchar = "UGSrwxRWX421"

newtest "listdir" --------------------------------------------------------------

do case "errors"

	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(system.listdir, "..", char))
		end
	end

	local path = ".."
	local iterator, state, init, closing
	spawn(function ()
		asserterr("not a directory", pcall(system.listdir, "file.lua", "~"))
	end)
	system.run()

	asserterr("not a directory", pcall(system.listdir, "file.lua", "~"))

	done()
end

do case "list contents"

	local path = ".."
	local iterator, state, init, closing
	spawn(function ()
		iterator, state, init, closing = system.listdir(path)
	end)
	system.run()

	local function newexpected()
		return {
			[".git"] = "directory",
			demo = "directory",
			doc = "directory",
			LICENSE = "file",
			lua = "directory",
			["README.md"] = "file",
			src = "directory",
			test = "directory",
		}
	end

	local expected = newexpected()
	for name, ftype in iterator, state, init, closing do
		assert(expected[name] == ftype)
		expected[name] = nil
	end
	assert(next(expected) == nil)

	local expected = newexpected()
	for name, ftype in system.listdir(path, "~") do
		assert(expected[name] == ftype)
		expected[name] = nil
	end
	assert(next(expected) == nil)

	done()
end

newtest "openfile" -------------------------------------------------------------

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
			if not string.find("~"..validmodes, char, 1, true) then
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
		for i = 1, 255 do
			local char = string.char(i)
			if not permchar:find(char, 1, true) then
				asserterr("unknown perm char", pcall(system.openfile, "./non-existent.txt", "N", char))
			end
		end
	end)
	system.run()

	done()
end

newtest "file:read" ------------------------------------------------------------

do case "errors"

	local file = assert(system.openfile("../LICENSE", "~w"))
	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(file.read, file, buffer, nil, nil, nil, char))
		end
	end

	asserterr("bad file descriptor", file:read(buffer, nil, nil, nil, "~"))

	file:close()

	assert(not buffer:diff(string.rep("\0", #buffer)))

	done()
end

do case "read contents"

	local file
	spawn(function ()
		file = assert(system.openfile("../LICENSE"))
		assert(file:read(buffer) == #buffer)
		assert(not buffer:diff("Copyright (C) 2017  "))
		assert(file:read(buffer, 11, 20, 57) == 10)
		assert(not buffer:diff("Copyright Permission"))
	end)
	system.run()

	assert(file:read(buffer, 1, 12, nil, "~") == 12)
	assert(not buffer:diff("Renato Maia rmission"))
	assert(file:read(buffer, 13, 20, 10, "~") == 8)
	assert(not buffer:diff("Renato Maia (C) 2017"))

	file:close()

	done()
end

newtest "file:write" -----------------------------------------------------------

do case "errors"

	local file = assert(system.openfile("../LICENSE", "~r"))
	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(file.write, file, "foo bar", nil, nil, nil, char))
		end
	end

	asserterr("bad file descriptor", file:write("foo bar", nil, nil, nil, "~"))

	file:close()

	done()
end

do case "write contents"

	local path = "DELETEME.txt"
	local file
	spawn(function ()
		file = assert(system.openfile(path, "wN", system.filebits.ruser))
		assert(file:write("Hello, World!") == 13)
		assert(file:write("Good Bye World! Later.", 6, 15, 20) == 10)
	end)
	system.run()

	assert(file:write(" Well. ", 1, -1, nil, "~") == 7)
	assert(file:write(" Gone.", 1, -1, 30, "~") == 6)

	file:close()

	file = assert(io.open(path))
	assert(file:read("a") == "Hello, World! Well. Bye World! Gone.")
	file:close()
	os.remove(path)

	done()
end

--------------------------------------------------------------------------------

local common = {
	boolean = "UGSrwxRWX421",
	number = "Md#*ugDBib_vamsc",
	string = "?",
}
local values = {}
local path = "file.lua"
local file = assert(system.openfile(path, "~"))

for _, spec in ipairs{
	{
		name = "fileinfo",
		func = system.fileinfo,
		arg = path,
		extra = {
			number = "NITFAtf",
			string = "@p",
			["nil"] = "=",
		},
		prefix = "l",
	},
	{
		name = "file:info",
		func = file.info,
		arg = file,
	},
} do

	newtest(spec.name)

	local typemap = {}
	for ltype, chars in pairs(common) do
		typemap[ltype] = chars
	end
	if spec.extra then
		for ltype, chars in pairs(spec.extra) do
			typemap[ltype] = (typemap[ltype] or "")..chars
		end
	end
	local options = {}
	for _, chars in pairs(typemap) do
		table.insert(options, chars)
	end
	options = table.concat(options)

	do case "errors"
		for i = 1, 255 do
			local char = string.char(i)
			if spec.prefix and string.find(spec.prefix, char, 1, "plain search") then
				asserterr("'"..char.."' must be in the begin of 'mode'",
				          pcall(spec.func, spec.arg, options..spec.prefix))
			elseif not string.find("~"..options, char, 1, "plain search") then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(spec.func, spec.arg, char))
			end
		end

		asserterr("unknown mode char (got '\255')",
		          pcall(spec.func, spec.arg, options.."\255"))

		asserterr("unable to yield", pcall(spec.func, spec.arg, options))

		done()
	end

	do case "single value"
		for c in string.gmatch(options , ".") do
			local ltype = type(spec.func(spec.arg, "~"..c))
			assert(string.find(typemap[ltype], c, 1, "plain"))

			local v1, v2, v3 = spec.func(spec.arg, "~"..c..c..c)

			assert(type(v1) == ltype)
			assert(v2 == v1)
			assert(v3 == v1)
		end

		done()
	end

	do case "all values"
		local vararg = require "vararg"
		local packed
		spawn(function ()
			packed = vararg.pack(spec.func(spec.arg, options))
		end)

		assert(packed == nil)
		assert(system.run() == false)

		assert(packed("#") == #options)
		for i = 1, #options do
			local c = options:sub(i, i)
			local value = packed(i)
			local ltype = type(value)
			assert(string.find(typemap[ltype], c, 1, "plain"))

			local previous = values[c]
			if previous == nil then
				values[c] = value
			else
				assert(previous == value)
			end
		end

		done()
	end

end

--------------------------------------------------------------------------------

local function timeequals(a, b)
	return math.abs(a-b) <= 1e-6
end

for _, spec in ipairs{
	{
		name = "touchfile",
		func = system.touchfile,
		arg = path,
		get = function (path) return system.fileinfo(path, "~am") end,
		prefix = "l",
	},
	{
		name = "file:touch",
		func = file.touch,
		arg = file,
		get = function (file) return file:info("~am") end,
	},
} do

	newtest(spec.name)

	local options = "amb"

	local access, modify = spec.get(spec.arg)

	do case "errors"
		for i = 1, 255 do
			local char = string.char(i)
			if spec.prefix and string.find(spec.prefix, char, 1, "plain search") then
				asserterr("'"..char.."' must be in the begin of 'mode'",
				          pcall(spec.func, spec.arg, options..spec.prefix, 0, 0, 0, 0))
			elseif not string.find("~"..options, char, 1, "plain search") then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(spec.func, spec.arg, char, 0))
			end
		end

		asserterr("unknown mode char (got '\255')",
		          pcall(spec.func, spec.arg, options.."\255", 0, 0, 0, 0))

		asserterr("unable to yield", pcall(spec.func, spec.arg, options, 0, 0, 0))

		local newaccess, newmodify = spec.get(spec.arg)
		assert(newaccess == access)
		assert(newmodify == modify)

		done()
	end

	do case "change times"
		local function testchange(mode)
			assert(spec.func(spec.arg, mode) == true)

			local access1, modify1 = spec.get(spec.arg)
			assert(access1 > access)
			assert(modify1 > modify)

			assert(spec.func(spec.arg, mode.."a", access) == true)

			local access2, modify2 = spec.get(spec.arg)
			assert(timeequals(access2, access))
			assert(modify2 > modify1)

			assert(spec.func(spec.arg, mode.."m", modify) == true)

			local access3, modify3 = spec.get(spec.arg)
			assert(access3 > access2)
			assert(timeequals(modify3, modify))

			assert(spec.func(spec.arg, mode.."b", access1) == true)

			local access4, modify4 = spec.get(spec.arg)
			assert(timeequals(access4, access1))
			assert(timeequals(modify4, access1))

			assert(spec.func(spec.arg, mode.."am", access, modify) == true)

			local access5, modify5 = spec.get(spec.arg)
			assert(timeequals(access5, access))
			assert(timeequals(modify5, modify))
		end

		testchange("~")

		spawn(testchange, "")
		assert(system.run() == false)

		done()
	end

end

--------------------------------------------------------------------------------

for _, spec in ipairs{
	{
		name = "ownfile",
		func = system.ownfile,
		arg = path,
		get = function (path) return system.fileinfo(path, "~ug") end,
		prefix = "l",
	},
	{
		name = "file:own",
		func = file.own,
		arg = file,
		get = function (file) return file:info("~ug") end,
	},
} do

	newtest(spec.name)

	local uid, gid = spec.get(spec.arg)

	do case "errors"
		for i = 1, 255 do
			local char = string.char(i)
			if char ~= "~" and (spec.prefix == nil or not string.find(spec.prefix, char, 1, "plain search")) then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(spec.func, spec.arg, uid, gid, char))
			end
		end

		asserterr("number expected", pcall(spec.func, spec.arg))
		asserterr("number expected", pcall(spec.func, spec.arg, uid))
		asserterr("unable to yield", pcall(spec.func, spec.arg, uid, gid))

		local newuid, newgid = spec.get(spec.arg)
		assert(newuid == uid)
		assert(newgid == gid)

		done()
	end

	do case "change owner"
		local function testchange(mode)
			local res, errmsg = spec.func(spec.arg, 0, 0, mode)
			if res then
				assert(res == true)

				local curruid, currgid = spec.get(spec.arg)
				assert(curruid == 0)
				assert(currgid == 0)

				assert(spec.func(spec.arg, uid, -1, mode) == true)

				local curruid, currgid = spec.get(spec.arg)
				assert(curruid == uid)
				assert(currgid == 0)

				assert(spec.func(spec.arg, -1, gid, mode) == true)

				local curruid, currgid = spec.get(spec.arg)
				assert(curruid == uid)
				assert(currgid == gid)
			else
				asserterr("not permitted", res, errmsg)
			end
		end

		testchange("~")

		spawn(testchange, "")
		assert(system.run() == false)

		done()
	end

end

--------------------------------------------------------------------------------

for _, spec in ipairs{
	{
		name = "grantfile",
		func = system.grantfile,
		arg = path,
		get = function (path) return system.fileinfo(path, "~M") & ~system.filebits.type end,
	},
	{
		name = "file:grant",
		func = file.grant,
		arg = file,
		get = function (file) return file:info("~M") & ~system.filebits.type end,
	},
} do

	newtest(spec.name)

	local filemode = spec.get(spec.arg)

	do case "errors"
		for i = 1, 255 do
			local char = string.char(i)
			if char ~= "~" then
				asserterr("unknown mode char (got '"..char.."')",
				          pcall(spec.func, spec.arg, 0, char))
			end
			if not permchar:find(char, 1, "plain search") then
				asserterr("unknown perm char (got '"..char.."')",
				          pcall(spec.func, spec.arg, char, "~"))
			end
		end

		asserterr("number expected", pcall(spec.func, spec.arg))
		asserterr("unable to yield", pcall(spec.func, spec.arg, 0))

		local newfilemode = spec.get(spec.arg)
		assert(newfilemode == filemode)

		done()
	end

	do case "change permissions"
		local function testchange(mode)
			local newmode = system.filebits.ruser|system.filebits.wuser
			assert(spec.func(spec.arg, newmode, mode))
			assert(spec.get(spec.arg) == newmode)

			assert(spec.func(spec.arg, filemode, mode) == true)
			assert(spec.get(spec.arg) == filemode)
		end

		testchange("~")

		spawn(testchange, "")
		assert(system.run() == false)

		done()
	end

end

file:close()
