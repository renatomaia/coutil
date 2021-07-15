local system = require "coutil.system"

local luabin = "lua"
do
	local i = -1
	while arg[i] ~= nil do
		luabin = arg[i]
		i = i-1
	end
end

function writeto(path, ...)
	local file = assert(io.open(path, "w"))
	assert(file:write(...))
	assert(file:close())
end

function readfrom(path)
	local file = io.open(path, "r")
	if file ~= nil then
		local data = assert(file:read("*a"))
		assert(file:close())
		return data
	end
end

local runscript
do
	local scriptfile = os.tmpname()
	local successfile = os.tmpname()

	local function fillenv(env)
		if env ~= nil then
			env.PATH = os.getenv("PATH")
			env.LUA_INIT = os.getenv("LUA_INIT")
			env.LUA_PATH = os.getenv("LUA_PATH")
			env.LUA_CPATH = os.getenv("LUA_CPATH")
			env.LD_LIBRARY_PATH = os.getenv("LD_LIBRARY_PATH")
		end
		return env
	end

	function runscript(info, ...)
		local command = luabin
		local script = info
		if type(info) == "table" then
			script = info.script
			local arguments = {scriptfile}
			if info.arguments ~= nil then
				for index, argval in ipairs(info.arguments) do
					arguments[index+1] = argval
				end
			end
			command = {
				execfile = command,
				runpath = info.runpath,
				environment = fillenv(info.environment),
				arguments = arguments,
				stdin = info.stdin,
				stdout = info.stdout,
				stderr = info.stderr,
			}
		end
		writeto(scriptfile, [[
			local function main(...) ]], script, [[ end
			local exitval = main(...)
			local file = assert(io.open("]],successfile,[[", "w"))
			assert(file:write("SUCCESS!"))
			assert(file:close())
			os.exit(exitval)
		]])
		local ended, exitval = system.execute(command, scriptfile, ...)
		assert(ended == "exit")
		assert(type(command) ~= "table" or type(command.pid) == "number")
		assert(readfrom(successfile) == "SUCCESS!")
		assert(os.remove(scriptfile))
		os.remove(successfile)
		return exitval
	end
end

newtest "execute" ------------------------------------------------------------------

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

do case "environment redefined"
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
			"bad name '=ENV' in field 'environment' (must be a string without '=')",
			pcall(system.execute, {
				execfile = luabin,
				environment = { ["=ENV"] = "illegal" },
			})
		)
	end)
	assert(system.run() == false)

	done()
end

do case "exit value"
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

local function teststdfiles(info)
	local infile = os.tmpname()
	local outfile = os.tmpname()
	local errfile = os.tmpname()
	if info.input == "r" then writeto(infile, "stdin") end
	if info.output == "r" then writeto(outfile, "stdout") end
	if info.error == "r" then writeto(errfile, "stderr") end
	local stdin = io.open(infile, info.input)
	local stdout = io.open(outfile, info.output)
	local stderr = io.open(errfile, info.error)
	runscript{
		script = info.script,
		stdin = stdin,
		stdout = stdout,
		stderr = stderr,
	}
	stdin:close()
	stdout:close()
	stderr:close()
	if info.input == "w" then assert(readfrom(infile) == "stdin") end
	if info.output == "w" then assert(readfrom(outfile) == "stdout") end
	if info.error == "w" then assert(readfrom(errfile) == "stderr") end
	os.remove(infile)
	os.remove(outfile)
	os.remove(errfile)
end

do case "redirect all streams"
	spawn(function ()
		teststdfiles{
			script = [[
				assert(io.stdin:read("a") == "stdin")
				assert(io.stdout:write("stdout"))
				assert(io.stderr:write("stderr"))
			]],
			input = "r",
			output = "w",
			error = "w",
		}
	end)
	assert(system.run() == false)

	done()
end
