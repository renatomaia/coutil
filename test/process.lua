local system = require "coutil.system"

local function fillenv(env)
	if type(env) == "table" then
		env.PATH = os.getenv("PATH")
		env.LUA_INIT = os.getenv("LUA_INIT")
		env.LUA_PATH = os.getenv("LUA_PATH")
		env.LUA_CPATH = os.getenv("LUA_CPATH")
		env.LD_LIBRARY_PATH = os.getenv("LD_LIBRARY_PATH")
	end
	return env
end

local runscript
do
	local scriptfile = os.tmpname()
	local successfile = os.tmpname()

	function runscript(info, ...)
		local command = luabin
		local script = info
		if type(info) == "table" then
			script = info.script
			if info.arguments == nil then
				info.arguments = {scriptfile}
			else
				for i = #info.arguments, 1, -1 do
					info.arguments[i+1] = info.arguments[i]
				end
				info.arguments[1] = scriptfile
			end
			info.execfile = command
			info.environment = fillenv(info.environment)
			command = info
		end
		writeto(scriptfile, string.format([[
			local function main(...) %s end
			local exitval = main(...)
			local file = assert(io.open(%q, "w"))
			assert(file:write("SUCCESS!"))
			assert(file:close())
			os.exit(exitval, true)
		]], script, successfile))
		local ended, exitval = system.execute(command, scriptfile, ...)
		assert(ended == "exit")
		assert(type(command) ~= "table" or type(command.pid) == "number")
		assert(readfrom(successfile) == "SUCCESS!")
		assert(os.remove(scriptfile))
		os.remove(successfile)
		return exitval
	end
end

newtest "g|setdir" ---------------------------------------------------------------

do case "change value"
	local path = system.getdir()
	assert(type(path) == "string")

	local homedir = system.procinfo("H")
	assert(system.setdir(homedir) == true)

	assert(system.getdir() == homedir)
	assert(system.setdir(path) == true)

	done()
end

newtest "g|setenv" ---------------------------------------------------------------

do case "invalid arguments"
	local acceptable = {
		string = true,
		number = true,
		table = false,
	}
	for _, value in ipairs(types) do
		local argok = acceptable[type(value)]
		if not argok then
			asserterr("string expected", pcall(system.setenv, "name", value))
			if argok == nil then
				asserterr("string expected", pcall(system.setenv, value, "value"))
				asserterr("string expected", pcall(system.getenv, value))
			end
		end
	end

	done()
end

do case "inexistent variable"
	asserterr("no such file or directory",
		system.getenv("COUTIL_TEST_MISSING"))
	assert(system.setenv("COUTIL_TEST_MISSING", nil))

	done()
end

do case "invalid variable names"
	asserterr("invalid argument", system.setenv("COUTIL_TEST=abc", "def"))
	asserterr("no such file or directory", system.getenv("COUTIL_TEST=abc"))
	asserterr("no such file or directory", system.getenv("COUTIL_TEST"))
	assert(os.getenv("COUTIL_TEST=abc") == nil)
	assert(os.getenv("COUTIL_TEST") == nil)

	assert(system.setenv("COUTIL_TEST", "abc=def"))
	assert(system.getenv("COUTIL_TEST") == "abc=def")
	if standard == "win32" then
		assert(os.getenv("COUTIL_TEST") == nil)
		asserterr("no such", system.getenv("COUTIL_TEST=abc"))
	else
		assert(os.getenv("COUTIL_TEST") == "abc=def")
		assert(os.getenv("COUTIL_TEST=abc") == "def")
		assert(system.getenv("COUTIL_TEST=abc") == "def")
	end

	done()
end

do case "large values"
	local value = string.rep("X", 1024)
	assert(system.setenv("COUTIL_TEST_HUGE", value))
	assert(system.getenv("COUTIL_TEST_HUGE") == value)
	assert(os.getenv("COUTIL_TEST_HUGE") == (standard == "posix" and value or nil))

	done()
end

do case "listing all variables"
	local env1 = assert(system.getenv())
	assert(type(env1) == "table")
	for name, value in pairs(env1) do
		assert(type(name) == "string")
		assert(type(value) == "string")
		assert(system.getenv(name) == value)

		if standard == "posix" then
			assert(os.getenv(name) == value)
		else
			local actual = os.getenv(name)
			assert(actual == value or actual == nil)
		end
	end

	local tab = { 1,2,3, extra = "unchanged", COUTIL_TEST = "oops!" }
	local env2 = system.getenv(tab)
	assert(env2 == tab)
	for name, value in pairs(env1) do
		assert(env2[name] == value)
	end
	assert(env2[1] == 1)
	assert(env2[2] == 2)
	assert(env2[3] == 3)
	assert(env2.extra == "unchanged")
	assert(env2.COUTIL_TEST ~= "oops!")

	done()
end

do case "clearing variable"
	assert(system.setenv("COUTIL_TEST", nil))
	asserterr("no such", system.getenv("COUTIL_TEST"))
	assert(os.getenv("COUTIL_TEST") == nil)

	done()
end

newtest "execute" --------------------------------------------------------------

do case "error messages"
	asserterr("unable to yield", pcall(system.execute))
	asserterr("unable to yield", pcall(system.execute, function () end))
	asserterr("unable to yield", pcall(system.execute, "echo", {}))
	asserterr("unable to yield", pcall(system.execute, "echo", "Hello, World!"))

	spawn(function ()
		asserterr("string expected", pcall(system.execute))
		asserterr("string expected", pcall(system.execute, function () end))
		asserterr("string expected", pcall(system.execute, "echo", {}))
	end)
	assert(system.run() == false)

	done()
end

do case "arguments"
	local argtests = {
		{
			script = [[
			assert(select("#", ...) == 1)
			assert(select(1, ...) == "f1")
			]],
			arguments = {"f1"},
		},
		{
			script = [[
			assert(select("#", ...) == 3)
			assert(select(1, ...) == "f1")
			assert(select(2, ...) == "f2")
			assert(select(3, ...) == "f3")
			]],
			arguments = {"f1", "f2", "f3"},
		},
		{
			script = [[
			assert(select("#", ...) == 1)
			assert(select(1, ...) == "f1 f2 f3")
			]],
			arguments = {"f1 f2 f3"},
		},
		{
			script = [[
			assert(select("#", ...) == 1)
			assert(select(1, ...) == '"f1", "f2", "f3"')
			]],
			arguments = {'"f1", "f2", "f3"'},
		},
		{
			script = [[
			assert(select("#", ...) == 1)
			assert(select(1, ...) == "f1")
			]],
			arguments = {"f1\0f2\0f3"},
		}
	}

	spawn(function ()
		for _, case in ipairs(argtests) do
			runscript(case.script, table.unpack(case.arguments))
			runscript(case)
		end
	end)
	assert(system.run() == false)

	done()
end

do case "exit value"
	spawn(function ()
		for index, code in ipairs{ 0, 1, 2, 3, 127, 128, 255 } do
			local script = "return "..code
			assert(runscript(script) == code)
			assert(runscript{ script = script } == code)
		end
	end)
	assert(system.run() == false)

	done()
end

do case "environment inherit"
	spawn(function ()
		runscript(string.format('assert(os.getenv("HOME") == %q)', os.getenv("HOME")))
	end)
	assert(system.run() == false)

	done()
end

do case "environment redefined with table"
	spawn(function ()
		runscript{
			script = [[
				assert(os.getenv("ENV_VAR_01") == "environment variable 01")
				assert(os.getenv("ENV_VAR_02") == "environment variable 02")
				assert(os.getenv("ENV_VAR_03") == "environment variable 03")
				assert(os.getenv("HOME") == nil)
			]],
			environment = {
				ENV_VAR_01 = "environment variable 01",
				ENV_VAR_02 = "environment variable 02",
				ENV_VAR_03 = "environment variable 03",
			},
		}
	end)
	assert(system.run() == false)

	done()
end

do case "environment redefined with environment"
	spawn(function ()
		runscript{
			script = [[
				assert(os.getenv("ENV_VAR_01") == "environment variable 01")
				assert(os.getenv("ENV_VAR_02") == "environment variable 02")
				assert(os.getenv("ENV_VAR_03") == "environment variable 03")
				assert(os.getenv("HOME") == nil)
			]],
			environment = system.packenv(fillenv{
				ENV_VAR_01 = "environment variable 01",
				ENV_VAR_02 = "environment variable 02",
				ENV_VAR_03 = "environment variable 03",
			}),
		}
	end)
	assert(system.run() == false)

	done()
end

do case "environment unpacked"
	local environment = system.packenv{
		ENV_VAR_01 = "environment variable 01",
		ENV_VAR_02 = "environment variable 02",
		ENV_VAR_03 = "environment variable 03",
	}
	assert(environment.ENV_VAR_01 == "environment variable 01")
	assert(environment.ENV_VAR_02 == "environment variable 02")
	assert(environment.ENV_VAR_03 == "environment variable 03")
	assert(environment.ENV_VAR_04 == nil)

	local created = system.unpackenv(environment)
	assert(created.ENV_VAR_01 == "environment variable 01")
	assert(created.ENV_VAR_02 == "environment variable 02")
	assert(created.ENV_VAR_03 == "environment variable 03")
	created.ENV_VAR_01 = nil
	created.ENV_VAR_02 = nil
	created.ENV_VAR_03 = nil
	assert(next(created) == nil)

	created.extra = "extra"
	created[1] = "one"
	local repacked = system.unpackenv(environment, created)
	assert(rawequal(created, repacked))
	assert(created.ENV_VAR_01 == "environment variable 01")
	assert(created.ENV_VAR_02 == "environment variable 02")
	assert(created.ENV_VAR_03 == "environment variable 03")
	assert(created.extra == "extra")
	assert(created[1] == "one")
	created.ENV_VAR_01 = nil
	created.ENV_VAR_02 = nil
	created.ENV_VAR_03 = nil
	created.extra = nil
	created[1] = nil
	assert(next(created) == nil)

	done()
end

do case "environment ignored null"
	spawn(function ()
		runscript{
			script = [[
				assert(string.match(os.getenv("ENV_VAR"), "^environment variable 0[123]$"))
				assert(os.getenv("ENV_VAR_01") == nil)
				assert(os.getenv("ENV_VAR_02") == nil)
				assert(os.getenv("ENV_VAR_03") == nil)
				assert(os.getenv("HOME") == nil)
			]],
			environment = {
				["ENV_VAR\0_01"] = "environment variable 01",
				["ENV_VAR\0_02"] = "environment variable 02",
				["ENV_VAR\0_03"] = "environment variable 03",
			},
		}
	end)
	assert(system.run() == false)

	done()
end

do case "environment illegal char"
	spawn(function ()
		asserterr(
			"bad name '=ENV' in environment (cannot contain '=')",
			pcall(system.execute, {
				execfile = luabin,
				environment = { ["=ENV"] = "illegal" },
			})
		)
	end)
	assert(system.run() == false)

	done()
end

do case "environment redefined existing"
	spawn(function ()
		runscript{
			script = 'assert(os.getenv("HOME") == "My Home!")',
			environment = { ["HOME"] = "My Home!" },
		}
	end)
	assert(system.run() == false)

	done()
end

do case "relative run path"
	spawn(function ()
		runscript{
			script = [[
				local data = assert(io.open("testall.lua")):read("*a")
				assert(string.find(data, 'print "\\nSuccess!\\n"', 1, true))
			]],
			runpath = "../test",
		}
	end)
	assert(system.run() == false)

	done()
end

if standard == "posix" then
do case "absolute run path"
	spawn(function ()
		runscript{
			script = 'assert(io.open("deleteme.tmp", "w")):write("Delete me now!")',
			runpath = "/tmp",
		}
		assert(readfrom("/tmp/deleteme.tmp") == "Delete me now!")
		os.remove("/tmp/deleteme.tmp")
	end)
	assert(system.run() == false)

	done()
end
end

do case "invalid run path"
	spawn(function ()
		asserterr("no such file or directory", system.execute{
			execfile = luabin,
			arguments = { "-e", "os.exit()" },
			runpath = "nonexistent",
		})
	end)
	assert(system.run() == false)

	done()
end

do case "invalid executable path"
	spawn(function ()
		asserterr("no such file or directory", system.execute("nonexistent"))
	end)
	assert(system.run() == false)

	done()
end

do case "redirected streams"
	spawn(function ()
		local tempfile = os.tmpname()
		local outcases = {
			"Hello, World!",
			"\0\1\2\3",
			[[
				one single line
				another additional line
				yet even another final line
			]],
		}
		for _, case in ipairs(outcases) do
			for _, output in ipairs{"stdout", "stderr"} do
				local file = io.open(tempfile, "w")
				runscript{
					script = string.format('io.%s:write(%q)', output, case),
					[output] = file,
				}
				file:close()
				assert(readfrom(tempfile) == case)
				os.remove(tempfile)
			end

			writeto(tempfile, case)
			local file = io.open(tempfile, "r")
			runscript{
				script = string.format('assert(io.stdin:read("*a") == %q)', case),
				stdin = file,
			}
			file:close()
			os.remove(tempfile)
		end
	end)
	assert(system.run() == false)

	done()
end

do case "redirect streams to files"
	spawn(function ()
		local infile = os.tmpname()
		local outfile = os.tmpname()
		local errfile = os.tmpname()
		writeto(infile, "stdin")
		local stdin = io.open(infile, "r")
		local stdout = io.open(outfile, "w")
		local stderr = io.open(errfile, "w")
		runscript{
			script = [[
				assert(io.stdin:read("a") == "stdin")
				assert(io.stdout:write("stdout")) io.stdout:flush()
				assert(io.stderr:write("stderr")) io.stderr:flush()
			]],
			stdin = stdin,
			stdout = stdout,
			stderr = stderr,
		}
		stdin:close()
		stdout:close()
		stderr:close()
		assert(readfrom(outfile) == "stdout")
		assert(readfrom(errfile) == "stderr")
		os.remove(infile)
		os.remove(outfile)
		os.remove(errfile)
	end)
	assert(system.run() == false)

	done()
end

if standard == "posix" then
do case "redirect streams to a socket"
	local memory = require "memory"

	local function testsocketstdstream(domain, address)
		spawn(function ()
			local passive = assert(system.socket("passive", domain))
			assert(passive:bind(address))
			assert(passive:getaddress("self", address))
			assert(passive:listen(2))
			local control = assert(passive:accept())
			assert(passive:close())
			assert(control:write("stdin"))
			assert(control:shutdown())
			local buffer = memory.create(12)
			local bytes = 0
			while bytes < #buffer do
				bytes = bytes+assert(control:read(buffer, bytes+1))
			end
			assert(tostring(buffer) == "stdoutstderr")
			assert(control:close())
		end)

		spawn(function ()
			local socket = assert(system.socket("stream", domain))
			assert(socket:connect(address))
			runscript{
				script = [[
					assert(io.stdin:read("a") == "stdin")
					assert(io.stdout:write("stdout")) io.stdout:flush()
					assert(io.stderr:write("stderr")) io.stderr:flush()
				]],
				stdin = socket,
				stdout = socket,
				stderr = socket,
			}
		end)

		assert(system.run() == false)
	end

	testsocketstdstream("ipv4", system.address("ipv4", "127.0.0.1:0"))
	testsocketstdstream("ipv6", system.address("ipv6", "[::1]:0"))

	local filepath = os.tmpname()
	os.remove(filepath)
	testsocketstdstream("local", filepath)
	os.remove(filepath)

	done()
end

do case "redirect streams to created pipe"
	local memory = require "memory"

	local spec = {
		script = [[
			assert(io.stdin:read("a") == "stdin")
			assert(io.stdout:write("stdout")) io.stdout:flush()
			assert(io.stderr:write("stderr")) io.stderr:flush()
		]],
		stdin = "w",
		stdout = "r",
		stderr = "r",
	}
	spawn(runscript, spec)

	spawn(function ()
		assert(spec.stdin:write("stdin"))
		assert(spec.stdin:shutdown())
		local buffer = memory.create(6)
		for name, stream in pairs{ stdout = spec.stdout, stderr = spec.stderr } do
			local bytes = 0
			while bytes < #buffer do
				bytes = bytes+assert(stream:read(buffer, bytes+1))
			end
			assert(tostring(buffer) == name)
			assert(stream:close())
		end
	end)

	assert(system.run() == false)

	done()
end
end

do case "redirect streams to null"
	local memory = require "memory"

	local spec = {
		script = [[
			assert(io.stdin:read("a") == "")
			local long = string.rep(" FIX ME! ", 1024)
			assert(io.stdout:write(long)) io.stdout:flush()
			assert(io.stderr:write(long)) io.stderr:flush()
		]],
		stdin = false,
		stdout = false,
		stderr = false,
	}
	spawn(runscript, spec)

	assert(system.run() == false)

	done()
end

if standard == "posix" then
do case "signal termination"
	for i, signal in ipairs{
		"TERMINATE",
		"terminate",
		"interrupt",
		"quit",
		"userdef1",
		"userdef2",
		32
	} do
		local procinfo = { execfile = "sleep", arguments = { "5" } }

		spawn(function ()
			local res, extra = system.execute(procinfo)
			if (type(signal) == "string") then
				assert(res == signal)
				assert(type(extra) == "number")
			else
				assert(res == "signal")
				assert(extra == signal)
			end
		end)

		spawn(function ()
			system.suspend(.1+i*.01)
			system.emitsig(procinfo.pid, signal)
		end)
	end

	assert(system.run() == false)

	done()
end
end

do case "yield values"
	local stage = 0
	local a,b,c = spawn(function ()
		local res, extra = system.execute(luabin, "-e", "return")
		assert(res == "exit")
		assert(extra == 0)
		stage = 1
	end)
	assert(a == nil)
	assert(b == nil)
	assert(c == nil)
	assert(stage == 0)
	gc()

	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		system.execute(luabin, "-e", "return")
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)
	gc()

	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule"
	local stage = 0
	spawn(function ()
		local res, extra = system.execute(luabin, "-e", "return")
		assert(res == "exit")
		assert(extra == 0)
		stage = 1
		local res, extra = system.execute(luabin, "-e", "return")
		assert(res == "exit")
		assert(extra == 0)
		stage = 2
	end)
	assert(stage == 0)

	if standard == "posix" then
		gc()
		assert(system.run("step") == true)
		assert(stage == 1)
	end

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local a,b,c = system.execute(luabin, "-e", "return")
		assert(a == true)
		assert(b == nil)
		assert(c == 3)
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro, true,nil,3)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "cancel and reschedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local res = system.execute(luabin, "-e", "return")
		assert(res == nil)
		stage = 1
		local res, extra = system.execute(luabin, "-e", "return")
		assert(res == "exit")
		assert(extra == 0)
		stage = 2
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "resume while closing"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		assert(system.execute(luabin, "-e", "return") == nil)
		stage = 1
		local a,b,c = system.execute(luabin, "-e", "return")
		assert(a == 1)
		assert(b == 22)
		assert(c == 333)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		coroutine.resume(garbage.coro, 1,22,333) -- while being closed.
		assert(stage == 2)
		stage = 3
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 3)

	done()
end

do case "ignore errors"
	local stage = 0
	pspawn(function ()
		system.execute(luabin, "-e", "return")
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"
	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		system.execute(luabin, "-e", "return")
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

newtest "g|setpriority" ---------------------------------------------------------------

do case "error messages"
	asserterr("out of range", pcall(system.setpriority, 123, -21))
	asserterr("out of range", pcall(system.setpriority, 123, 20))
	asserterr("out of range", pcall(system.setpriority, 123, math.maxinteger))
	asserterr("invalid option", pcall(system.setpriority, 123, "other"))

	done()
end

do case "own priority"
	local pid = system.procinfo("#")
	local name, value = system.getpriority(pid)
	assert(type(name) == "string")
	assert(type(value) == "number")

	if standard == "win32" then
		assert(system.setpriority(pid, "below"))

		local name, newval = system.getpriority(pid)
		assert(name == "below")
		assert(newval == 10)
	else
		assert(system.setpriority(pid, value+1))

		local name, newval = system.getpriority(pid)
		assert(name == "other")
		assert(newval == value+1)
	end


	done()
end

do case "change other process"
	local proc = { execfile = luabin, arguments = { "-v" } }
	spawn(function () assert(system.execute(proc)) end)

	assert(system.setpriority(proc.pid, "below"))
	assert(system.getpriority(proc.pid) == "below")

	assert(system.setpriority(proc.pid, "low"))
	assert(system.getpriority(proc.pid) == "low")

	assert(system.emitsig(proc.pid, "terminate"))

	assert(system.run() == false)

	done()
end

