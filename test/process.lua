local system = require "coutil.system"

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
			os.exit(exitval, true)
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

do case "environment redefined new"
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
			assert(control:send("stdin"))
			assert(control:shutdown())
			local buffer = memory.create(12)
			local bytes = 0
			while bytes < #buffer do
				bytes = bytes+assert(control:receive(buffer, bytes+1))
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

do case "signal termination"
	for i, signal in ipairs{
		"terminate",
		"interrupt",
		"quit",
		"user1",
		"user2",
	} do
		local procinfo = { execfile = "sleep", arguments = { "5" } }

		spawn(function ()
			local res, extra = system.execute(procinfo)
			assert(res == signal)
			assert(type(extra) == "number")
		end)

		spawn(function ()
			system.suspend(.1+i*.01)
			system.emitsig(procinfo.pid, signal)
		end)
	end

	assert(system.run() == false)

	done()
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

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

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
