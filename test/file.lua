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

	spawn(function ()
		asserterr("not a directory", pcall(system.listdir, "file.lua"))
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
			[".github"] = "directory",
			demo = "directory",
			doc = "directory",
			etc = "directory",
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

newtest "makedir" --------------------------------------------------------------

do case "errors"
	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(system.makedir, "DELETEME", tonumber("700", 8), char))
		end
	end

	asserterr("already exists", system.makedir("benchmarks", tonumber("700", 8), "~"))

	done()
end

do case "create directory"
	local path = "DELETEME.DIR"

	local function testcase(mode)
		assert(system.makedir(path, tonumber("750", 8), mode) == true)
		assert(system.fileinfo(path, mode.."?") == "directory")
		assert(system.removefile(path, mode.."d"))
	end

	testcase("~")

	spawn(testcase, "")
	assert(system.run() == false)

	done()
end

newtest "maketemp" --------------------------------------------------------------

do case "errors"

	asserterr("'~' must be in the begin of 'mode'",
	          pcall(system.maketemp, "abc", "f~o"))

	for i = 1, 255 do
		local char = string.char(i)
		if not string.find("~fo", char, 1, true) then
			asserterr("unknown mode char", pcall(system.maketemp, "lcutest_", char))
		end
	end

	local toolong = string.rep("X", 250)
	spawn(function ()
		asserterr("too long", pcall(system.maketemp, toolong))
	end)
	system.run()

	asserterr("too long", pcall(system.maketemp, toolong, "~"))

	done()
end

do
	local testcases = {}
	function testcases.dir(mode)
		local path, extra = system.maketemp("lcutest_", mode)
		assert(string.match(path, "lcutest_......$"))
		assert(extra == nil)
		assert(system.removefile(path, mode.."d"))
	end
	function testcases.file(mode)
		local path, extra = system.maketemp("lcutest_", mode.."f")
		assert(string.match(path, "lcutest_......$"))
		assert(extra == nil)
		assert(system.removefile(path, mode))
	end
	function testcases.open(mode)
		local file, extra = system.maketemp("lcutest_", mode.."o")
		assert(file:close(mode))
		assert(extra == nil)
		os.execute((standard == "win32" and "del" or "rm").." lcutest_??????")
	end
	function testcases.fileopen(mode)
		local path, file = system.maketemp("lcutest_", mode.."fo")
		assert(string.match(path, "lcutest_......$"))
		assert(file:close(mode))
		assert(system.removefile(path, mode))
	end
	function testcases.openfile(mode)
		local file, path = system.maketemp("lcutest_", mode.."of")
		assert(string.match(path, "lcutest_......$"))
		assert(file:close(mode))
		assert(system.removefile(path, mode))
	end
	function testcases.manyrets(mode)
		local p1, f1, f2, p2 = system.maketemp("lcutest_", mode.."foof")
		assert(rawequal(p1, p2))
		assert(rawequal(f1, f2))
		assert(string.match(p1, "lcutest_......$"))
		assert(f1:close(mode))
		assert(system.removefile(p1, mode))
	end

	for casename, casefunc in pairs(testcases) do
		case("create "..casename)

		casefunc("~")

		spawn(casefunc, "")
		assert(system.run() == false)

		done()
	end
end

newtest "linkfile" --------------------------------------------------------------

do case "errors"

	for i = 1, 255 do
		local char = string.char(i)
		if not string.find("~sdj", char, 1, true) then
			asserterr("unknown mode char", pcall(system.linkfile, "file.lua", "link.lua", char))
		end
	end

	local function testerr(mode)
		local expectederrmsg = "already exists"
		if not mode:find("s", 1, "plain search") then
			asserterr("no such", system.linkfile("LICENSE", "link.lua", mode))
		end
		asserterr(expectederrmsg, system.linkfile("../LICENSE", "file.lua", mode))
	end

	testerr("~")
	spawn(testerr, "")
	assert(system.run() == false)

	testerr("~s")
	spawn(testerr, "s")
	assert(system.run() == false)

	done()
end

do
	local testcases = {}
	function testcases.hardlink(mode)
		assert(system.linkfile("file.lua", "link.lua", mode) == true)
		assert(system.fileinfo("link.lua", mode.."?") == "file")
		if standard == "win32" then
			asserterr("unknown error", pcall(system.fileinfo, "link.lua", mode.."=")) -- TODO: check if this is a bug in libuv
		else
			assert(system.fileinfo("link.lua", mode.."=") == nil)
		end
		assert(system.fileinfo("link.lua", mode.."*") == 2)
		assert(system.fileinfo("file.lua", mode.."*") == 2)
		assert(system.removefile("link.lua", mode))
		assert(system.fileinfo("file.lua", mode.."*") == 1)
	end
	function testcases.symbolic(mode)
		assert(system.linkfile("file.lua", "link.lua", "s"..mode) == true)
		assert(system.fileinfo("link.lua", mode.."l?") == "link")
		assert(system.fileinfo("link.lua", mode.."?") == "file")
		assert(system.fileinfo("link.lua", mode.."=") == "file.lua")
		assert(system.removefile("link.lua", mode))
	end
	function testcases.directory(mode)
		assert(system.linkfile("benchmarks", "link.dir", "d"..mode) == true)
		assert(system.fileinfo("link.dir", mode.."l?") == "link")
		assert(system.fileinfo("link.dir", mode.."?") == "directory")
		assert(system.fileinfo("link.dir", mode.."=") == "benchmarks")
		assert(system.removefile("link.dir", mode))
	end

	for casename, casefunc in pairs(testcases) do
		case("create "..casename)

		casefunc("~")

		spawn(casefunc, "")
		assert(system.run() == false)

		done()
	end
end

newtest "movefile" --------------------------------------------------------------

do case "errors"

	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(system.movefile, "file.lua", "moved.lua", char))
		end
	end

	local function testerr(mode)
		asserterr("no such", system.movefile("LICENSE", "link.lua", mode))
		if standard ~= "win32" then
			asserterr("not a directory", system.movefile("benchmarks", "file.lua", mode))
		end
	end

	testerr("~")
	spawn(testerr, "")
	assert(system.run() == false)

	done()
end

do
	local testcases = {}
	function testcases.file(mode)
		assert(system.movefile("../LICENSE", "license.txt", mode) == true)
		local filesize = standard == "win32" and 1103 or 1085
		assert(system.fileinfo("license.txt", mode.."B") == filesize)
		assert(system.movefile("license.txt", "../LICENSE", mode) == true)
	end
	function testcases.directory(mode)
		assert(system.movefile("benchmarks", "renamed.dir", mode) == true)
		assert(system.fileinfo("renamed.dir", mode.."?") == "directory")
		assert(system.movefile("renamed.dir", "benchmarks", mode) == true)
	end

	for casename, testcase in pairs(testcases) do
		case("move "..casename)

		spawn(testcase, "")
		assert(system.run() == false)
		testcase("~")

		done()
	end
end

newtest "copyfile" --------------------------------------------------------------

do case "errors"

	for i = 1, 255 do
		local char = string.char(i)
		if not string.find("~ncC", char, 1, "plain search") then
			asserterr("unknown mode char", pcall(system.copyfile, "file.lua", "copied.lua", char))
		end
	end

	local function testerr(mode)
		asserterr("no such", system.copyfile("LICENSE", "copied.lua", mode))
		asserterr("already exists", system.copyfile("../LICENSE", "file.lua", mode.."n"))
		asserterr(standard == "win32" and "operation not permitted" or "directory",
			system.copyfile("benchmarks", "copied.dir", mode))
	end

	testerr("~")
	spawn(testerr, "")
	assert(system.run() == false)

	done()
end

do case "copy contents"
	local function testcase(mode)
		assert(system.copyfile("../LICENSE", "copied.txt", mode) == true)
		assert(system.fileinfo("copied.txt", mode.."B") == system.fileinfo("../LICENSE", mode.."B"))
		assert(system.copyfile("file.lua", "copied.txt", mode) == true)
		assert(system.fileinfo("copied.txt", mode.."B") == system.fileinfo("file.lua", mode.."B"))
		assert(system.removefile("copied.txt", mode))
	end

	testcase("~")
	spawn(testcase, "")
	assert(system.run() == false)

	done()
end

newtest "removefile" -----------------------------------------------------------

do case "errors"

	for i = 1, 255 do
		local char = string.char(i)
		if not string.find("~d", char, 1, true) then
			asserterr("unknown mode char", pcall(system.removefile, "file.lua", char))
		end
	end

	local function testerr(mode)
		asserterr("no such", system.removefile("MISSING.FILE", mode))
		asserterr("no such", system.removefile("MISSING.DIR", "d"..mode))
		asserterr(standard == "win32" and "operation not permitted" or "directory",
			system.removefile("benchmarks", mode))
		asserterr("not empty", system.removefile("benchmarks", "d"..mode))
	end

	testerr("~")
	spawn(testerr, "")
	assert(system.run() == false)

	done()
end

do case "remove files"
	local function testcase(mode)
		assert(system.makedir("DELETEME.DIR", tonumber("700", 8), mode))
		assert(system.fileinfo("DELETEME.DIR", mode.."?") == "directory")
		assert(system.openfile("DELETEME.DIR/DELETEME.TXT", mode.."N", tonumber("600", 8)):close(mode))
		assert(system.fileinfo("DELETEME.DIR/DELETEME.TXT", mode.."?") == "file")
		assert(system.removefile("DELETEME.DIR/DELETEME.TXT", mode) == true)
		assert(system.removefile("DELETEME.DIR", "d"..mode) == true)
	end

	testcase("~")
	spawn(testcase, "")
	assert(system.run() == false)

	done()
end

newtest "openfile" -------------------------------------------------------------

local validpath = "file.lua"
local validmodes = "rwanNrftwx"

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
		assert(system.openfile(validpath, "w", "invalid")):close("~")
		assert(system.openfile(validpath, "w", "rw", "invalid")):close("~")
		assert(system.openfile(validpath, "w", "rw", "rw", "invalid")):close("~")
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

	local expectederrmsg = standard == "win32" and "operation not permitted" or "bad file descriptor"
	asserterr(expectederrmsg, file:read(buffer, nil, nil, nil, "~"))

	file:close("~")

	assert(not buffer:diff(string.rep("\0", #buffer)))

	done()
end

do case "read contents"

	local file
	spawn(function ()
		file = assert(system.openfile("../LICENSE"))
		assert(file:read(buffer) == #buffer)
		assert(not buffer:diff("Copyright (C) 2017-2"))
		local offset = standard == "win32" and 64 or 62  -- due to line breaks
		assert(file:read(buffer, 11, 20, offset) == 10)
		assert(not buffer:diff("Copyright Permission"))
	end)
	system.run()

	assert(file:read(buffer, 1, 12, nil, "~") == 12)
	assert(not buffer:diff("021  Renato rmission"))
	assert(file:read(buffer, 13, 20, 10, "~") == 8)
	assert(not buffer:diff("021  Renato (C) 2017"))

	file:close("~")

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

	local expectederrmsg = standard == "win32" and "operation not permitted" or "bad file descriptor"
	asserterr(expectederrmsg, file:write("foo bar", nil, nil, nil, "~"))

	file:close("~")

	done()
end

do case "from string"
	local path = "DELETEME.txt"
	local file = assert(system.openfile(path, "~wN", system.filebits.ruser))

	spawn(function ()
		assert(file:write("Hello, World!") == 13)
		assert(file:write("Good Bye World! Later.", 6, 15, 20) == 10)
	end)
	system.run()

	assert(file:write(" Well. ", 1, -1, nil, "~") == 7)
	assert(file:write(" Gone.", 1, -1, 30, "~") == 6)

	file:close("~")

	file = assert(io.open(path))
	assert(file:read("a") == "Hello, World! Well. Bye World! Gone.")
	file:close("~")
	assert(system.removefile(path, "~"))

	done()
end

do case "from file"
	local path = "DELETEME.txt"
	local file = assert(system.openfile(path, "~wN", system.filebits.ruser))
	local srcf = assert(system.openfile("../LICENSE", "~r"))

	spawn(function () assert(file:write(srcf, 1, 10) == 10) end)
	system.run()
	local first, last = 63, 72
	if standard == "win32" then
		first, last = first+2, last+2
	end
	assert(file:write(srcf, first, last, nil, "~") == 10)

	file = assert(io.open(path))
	assert(file:read("a") == "Copyright Permission")
	file:close("~")
	srcf:close("~")
	assert(system.removefile(path, "~"))

	done()
end

newtest "file:resize" --------------------------------------------------------

do case "errors"

	local file = assert(system.openfile("../LICENSE", "~r"))
	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(file.resize, file, 128, char))
		end
	end

	local expectederrmsg = standard == "win32" and "operation not permitted" or "invalid argument"
	asserterr(expectederrmsg, file:resize(32, "~"))

	file:close("~")

	done()
end

do case "resize contents"
	local path = "DELETEME.txt"

	local function testcase(mode)
		local file = assert(system.openfile(path, mode.."wN", system.filebits.ruser))
		for i = 1, 100 do
			assert(file:write("0123456789", nil, nil, nil, mode))
		end
		assert(file:flush(mode))
		assert(file:resize(256, mode) == true)
		assert(file:close(mode))
	end

	spawn(testcase, "")
	system.run()
	assert(system.fileinfo(path, "~B") == 256)
	assert(system.removefile(path, "~"))

	testcase("~")
	assert(system.fileinfo(path, "~B") == 256)
	assert(system.removefile(path, "~"))

	done()
end

newtest "file:flush" -----------------------------------------------------------

do case "errors"

	local file = assert(system.openfile("../LICENSE", "~r"))
	for i = 1, 255 do
		local char = string.char(i)
		if not string.find("~d", char, 1, "plain search") then
			asserterr("unknown mode char", pcall(file.flush, file, char))
		end
	end

	file:close("~")

	done()
end

do case "flush contents"

	local path = "DELETEME.txt"
	local file = assert(system.openfile(path, "~wN", system.filebits.ruser))

	function testcase(...)
		for i = 1, select("#", ...) do
			local mode = select(i, ...)
			assert(file:write("mode="..mode.." ", nil, nil, nil, "~"))
			assert(file:flush(mode) == true)
		end
	end
	spawn(testcase, "", "d")
	system.run()

	testcase("~", "~d")

	file:close("~")

	file = assert(io.open(path))
	assert(file:read("a") == "mode= mode=d mode=~ mode=~d ")
	file:close("~")
	assert(system.removefile(path, "~"))

	done()
end

newtest "file:close" -----------------------------------------------------------

do case "errors"

	local file = assert(system.openfile("../LICENSE", "~r"))
	for i = 1, 255 do
		local char = string.char(i)
		if char ~= "~" then
			asserterr("unknown mode char", pcall(file.close, file, char))
		end
	end
	file:close("~")

	asserterr("closed", pcall(file.close, file, "~"))
	spawn(function ()
		asserterr("closed", pcall(file.close, file))

		assert(system.openfile("../LICENSE"):close())
	end)
	assert(system.run() == false)

	done()
end

do case "flush contents"

	local path = "DELETEME.txt"
	local file = assert(system.openfile(path, "~wN", system.filebits.ruser))

	function testcase(...)
		for i = 1, select("#", ...) do
			local mode = select(i, ...)
			assert(file:write("mode="..mode.." ", nil, nil, nil, "~"))
			assert(file:flush(mode) == true)
		end
	end
	spawn(testcase, "", "d")
	system.run()

	testcase("~", "~d")

	file:close("~")

	file = assert(io.open(path))
	assert(file:read("a") == "mode= mode=d mode=~ mode=~d ")
	file:close("~")
	assert(system.removefile(path, "~"))

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
			if c == "=" and standard == "win32" then -- TODO: check if this is a bug in libuv
				asserterr("unknown error", pcall(spec.func, spec.arg, "~="))
			else
				local ltype = type(spec.func(spec.arg, "~"..c))
				assert(string.find(typemap[ltype], c, 1, "plain"))

				local v1, v2, v3 = spec.func(spec.arg, "~"..c..c..c)

				assert(type(v1) == ltype)
				assert(v2 == v1)
				assert(v3 == v1)
			end
		end

		done()
	end

	do case "all values"
		local options = standard == "win32" and options:gsub("[=p]", "") or options
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

local linkpath = "link.lua"
assert(system.linkfile(path, linkpath, "~s"))

for _, spec in ipairs{
	{
		name = "touchfile",
		func = system.touchfile,
		arg = path,
		get = function (path, mode) return system.fileinfo(path, mode.."am") end,
		prefix = "l",
	},
	{
		name = "file:touch",
		func = file.touch,
		arg = file,
		get = function (file, mode) return file:info(mode.."am") end,
	},
} do

	newtest(spec.name)

	local options = "amb"

	local access, modify = spec.get(spec.arg, "~")

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

		local newaccess, newmodify = spec.get(spec.arg, "~")
		assert(newaccess == access)
		assert(newmodify == modify)

		done()
	end

	do case "change times"
		local function testchange(block, link)
			local mode = block..link
			if standard == "win32" and spec.name == "file:touch" then -- TODO: check if this is a bug in libuv
				asserterr("operation not permitted", spec.func(spec.arg, mode))
			else
				assert(spec.func(spec.arg, mode) == true)

				local access1, modify1 = spec.get(spec.arg, mode)
				assert(access1 > access)
				assert(modify1 > modify)

				system.suspend(0.01, block)
				assert(spec.func(spec.arg, mode.."a", access) == true)

				local access2, modify2 = spec.get(spec.arg, mode)
				assert(timeequals(access2, access))
				assert(modify2 > modify1)

				system.suspend(0.01, block)
				assert(spec.func(spec.arg, mode.."m", modify) == true)

				local access3, modify3 = spec.get(spec.arg, mode)
				assert(access3 > access2)
				assert(timeequals(modify3, modify))

				assert(spec.func(spec.arg, mode.."b", access1) == true)

				local access4, modify4 = spec.get(spec.arg, mode)
				assert(timeequals(access4, access1))
				assert(timeequals(modify4, access1))

				assert(spec.func(spec.arg, mode.."am", access, modify) == true)

				local access5, modify5 = spec.get(spec.arg, mode)
				assert(timeequals(access5, access))
				assert(timeequals(modify5, modify))
			end
		end

		testchange("~", "")

		spawn(testchange, "", "")
		assert(system.run() == false)

		if spec.name == "touchfile" then
			spec.arg = linkpath
			access, modify = system.fileinfo(linkpath, "~lam")

			testchange("~", "l")

			spawn(testchange, "", "l")
			assert(system.run() == false)
		end

		done()
	end

end

assert(system.removefile(linkpath, "~"))

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

		spawn(function ()
			asserterr("number expected", pcall(spec.func, spec.arg))
			asserterr("number expected", pcall(spec.func, spec.arg, uid))
		end)
		assert(system.run() == false)

		asserterr("number expected", pcall(spec.func, spec.arg, nil, nil, "~"))
		asserterr("number expected", pcall(spec.func, spec.arg, uid, nil, "~"))
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
			local newmode = system.filebits.ruser|system.filebits.rgroup|system.filebits.rother
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

file:close("~")
