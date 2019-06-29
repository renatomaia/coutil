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

local startscript
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

	function startscript(info, ...)
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
		os.remove(successfile)
		local process = assert(system.execute(command, scriptfile, ...))
		return {
			assertexit = function (_, expected)
				if type(expected) == "string" then
					asserterr(expected, process:awaitexit())
				else
					assert(process:awaitexit() == (expected or 0))
					assert(readfrom(successfile) == "SUCCESS!")
				end
				assert(process:close())
				assert(os.remove(scriptfile))
				os.remove(successfile)
			end
		}
	end

	function runscript(...)
		return startscript(...):assertexit()
	end
end

newtest "execute" ------------------------------------------------------------------

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

do return end

do
	for index, code in ipairs{ 0, 1, 2, 3, 127, 128, 255 } do
		local script = "return "..code
		tests.startscript(script):assertexit(code)
		tests.startscript{ script = script }:assertexit(code)
	end
end

do
	tests.runscript(string.format('assert(os.getenv("HOME") == %q)', os.getenv("HOME")))

	tests.runscript{
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

	tests.runscript{
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

	tests.testerror(
		"bad name '=ENV' in field 'environment' (must be a string without '=')",
		process.create,
		{ execfile = tests.luabin, environment = { ["=ENV"] = "illegal" } })

	tests.runscript{
		script = 'assert(os.getenv("HOME") == "My Home!")',
		environment = { ["HOME"] = "My Home!" },
	}
end

do
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
			tests.runscript{
				script = string.format('io.%s:write(%q)', output, case),
				[output] = file,
			}
			file:close()
			assert(tests.readfrom(tempfile) == case)
			os.remove(tempfile)
		end

		tests.writeto(tempfile, case)
		local file = io.open(tempfile, "r")
		tests.runscript{
			script = string.format('assert(io.stdin:read("*a") == %q)', case),
			stdin = file,
		}
		file:close()
		os.remove(tempfile)
	end
end

do
	local function teststdfiles(info)
		local infile = os.tmpname()
		local outfile = os.tmpname()
		local errfile = os.tmpname()
		tests.writeto(infile, info.input)
		local stderr = io.open(errfile, "w")
		local stdout = io.open(outfile, "w")
		local stdin = io.open(infile, "r")
		tests.runscript{
			script = info.script,
			stdin = stdin,
			stdout = stdout,
			stderr = stderr,
		}
		stdin:close()
		stdout:close()
		stderr:close()
		assert(tests.readfrom(outfile) == info.output)
		assert(tests.readfrom(errfile) == info.errors)
		os.remove(infile)
		os.remove(outfile)
		os.remove(errfile)
	end

	teststdfiles{
		script = [[
			assert(io.stdin:read("*a") == "stdin")
			assert(io.stdout:write("stdout"))
			assert(io.stderr:write("stderr"))
		]],
		input = "stdin",
		output = "stdout",
		errors = "stderr",
	}

	teststdfiles{
		script = [[
			local tests = require "test.process.utils"
			tests.runscript{
				script = [=[
					assert(io.stdin:read("*a") == "stdin")
					assert(io.stdout:write("stdout"))
					assert(io.stderr:write("stderr"))
				]=],
				stdout = io.stderr,
				stderr = io.stdout,
			}
		]],
		input = "stdin",
		output = "stderr",
		errors = "stdout",
	}

	teststdfiles{
		script = [[
			local tests = require "test.process.utils"
			tests.runscript{
				script = [=[
					assert(io.stdin:write("stdin") == nil)   -- bad file descriptor
					local res = io.stdout:read("*a")
					assert(res == nil or res == "stdout")
					assert(io.stderr:write("stderr"))
				]=],
				stdin = io.stderr,
				stdout = io.stdin,
				stderr = io.stdout,
			}
		]],
		input = "stdout",
		output = "stderr",
		errors = "", -- TODO: shouldn't be "stdin"?
	}
end

do
	tests.runscript{
		script = [[
			local data = assert(io.open("testall.lua")):read("*a")
			assert(string.find(data, 'print("OK")', 1, true))
		]],
		runpath = "test",
	}

	tests.runscript{
		script = 'assert(io.open("deleteme.tmp", "w")):write("Delete me now!")',
		runpath = "/tmp",
	}
	assert(tests.readfrom("/tmp/deleteme.tmp") == "Delete me now!")
	os.remove("/tmp/deleteme.tmp")
end
