local system = require "coutil.system"
local preemptco = require "coutil.coroutine"

newtest "coroutine" ------------------------------------------------------------

do case "compilation errors"
	for v in ipairs(types) do
		local ltype = type(v)
		if ltype ~= "string" and ltype ~= "number" then
			asserterr("string or memory expected", pcall(preemptco.load, v))
			asserterr("string expected", pcall(preemptco.load, "", v))
			asserterr("string expected", pcall(preemptco.load, "", "", v))
		end
	end

	local bytecodes = string.dump(function () a = a+1 end)
	asserterr("attempt to load a binary chunk (mode is 't')",
	          preemptco.load(bytecodes, nil, "t"))
	asserterr("attempt to load a text chunk (mode is 'b')",
	          preemptco.load("a = a+1", "bytecodes", "b"))
	asserterr("syntax error", preemptco.load("invalid chunk"))

	done()
end

do case "runtime errors"
	local co = preemptco.load[[ return 1, 2, 3 ]]
	asserterr("unable to yield", pcall(system.resume, co))
	assert(co:status() == "normal")

	local co = preemptco.load([[
		require "_G"
		error "Oops!"
	]], "@bytecodes")

	local stage
	spawn(function ()
		stage = 0
		asserterr("bytecodes:2: Oops!", system.resume(co))
		assert(co:status() == "dead")
		asserterr("cannot resume dead coroutine", system.resume(co))
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		asserterr("cannot resume running coroutine", system.resume(co))
		stage = 1
		local res, errval = system.resume(preemptco.load[[
			require "_G"
			error(true)
		]])
		assert(res == false)
		assert(errval == true)
		stage = 3
	end)

	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2 or stage == 3)

	done()
end

do case "type errors"
	local stage = 0
	spawn(function ()
		local co = preemptco.load(string.dump(function ()
			require "_G"
			error{}
		end))
		assert(co:status() == "normal")
		asserterr("unable to transfer argument #2 (got table)", system.resume(co, table))
		assert(co:status() == "normal")
		asserterr("unable to transfer argument #3 (got function)", system.resume(co, 1, print))
		assert(co:status() == "normal")
		asserterr("unable to transfer argument #4 (got thread)", system.resume(co, 1, 2, coroutine.running()))
		assert(co:status() == "normal")
		asserterr("unable to transfer argument #5 (got userdata)", system.resume(co, 1, 2, 3, co))
		assert(co:status() == "normal")
		stage = 1
		asserterr("unable to transfer error (got table)", system.resume(co))
		assert(co:status() == "dead")
		co = preemptco.load[[ return 1, {2}, 3 ]]
		asserterr("unable to transfer return value #2 (got table)", system.resume(co))
		assert(co:status() == "dead")
		co = preemptco.load[[ return 1, 2, 3, require ]]
		asserterr("unable to transfer return value #4 (got function)", system.resume(co))
		assert(co:status() == "dead")
		stage = 2
	end)

	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "preemptive execution"
	local path = os.tmpname()
	os.remove(path)

	assert(io.open(path) == nil)
	local co = assert(preemptco.load(string.format([[
		local io = require "io"
		local os = require "os"
		local path = %q
		while true do
			local file = io.open(path)
			if file ~= nil then
				local contents = file:read("a")
				file:close()
				if contents  == "deleteme" then
					os.remove(path)
					break
				end
			end
		end
	]], path)))
	assert(co:status() == "normal")

	local stage = 0
	spawn(function ()
		assert(system.resume(co))
		stage = 1
	end)
	assert(co:status() == "running")

	local file = assert(io.open(path, "w"))
	file:write("deleteme")
	file:close()

	local removed = false
	for i=1, 1e3 do
		file = io.open(path)
		if file == nil then
			removed = true
			break
		else
			file:close()
		end
	end
	assert(removed)

	assert(co:status() == "running")
	assert(stage == 0)
	system.run()
	assert(stage == 1)
	assert(co:status() == "dead")

	done()
end

do case "transfer values"
	local co = preemptco.load[[
		require "_G"
		local coroutine = require "coroutine"

		assert(select("#", ...) == 5)
		local a,b,c,d,e = ...
		assert(a == nil)
		assert(b == false)
		assert(c == 123)
		assert(d == 0xfacep-8)
		assert(e == "\001")

		assert(select("#", coroutine.yield()) == 0)
		assert(coroutine.yield(nil) == nil)
		assert(coroutine.yield(false) == false)
		assert(coroutine.yield(true) == true)
		assert(coroutine.yield(0) == 0)
		assert(coroutine.yield(1) == 1)
		assert(coroutine.yield(-1) == -1)
		assert(coroutine.yield(0xadap-16) == 0xadap-16)
		assert(coroutine.yield('') == '')
		assert(coroutine.yield('Lua 5.4') == 'Lua 5.4')
		assert(coroutine.yield('\\0') == '\\0')

		return ...
	]]
	assert(co:status() == "normal")

	spawn(function ()
		local function assertreturn(count, ...)
			assert(co:status() == count == 1 and "suspended" or "dead")
			assert(select("#", ...) == 2*count+1)
			assert(select(count+1, ...) == true)
			for i = 1, count do
				assert(select(i, ...) == select(count+1+i, ...))
			end
		end
		assertreturn(0, system.resume(co, nil, false, 123, 0xfacep-8, "\001"))
		assertreturn(1, nil, system.resume(co))
		assertreturn(1, false, system.resume(co, nil))
		assertreturn(1, true, system.resume(co, false))
		assertreturn(1, 0, system.resume(co, true))
		assertreturn(1, 1, system.resume(co, 0))
		assertreturn(1, -1, system.resume(co, 1))
		assertreturn(1, 0xadap-16, system.resume(co, -1))
		assertreturn(1, '', system.resume(co, 0xadap-16))
		assertreturn(1, 'Lua 5.4', system.resume(co, ''))
		assertreturn(1, '\\0', system.resume(co, 'Lua 5.4'))
		assertreturn(5, nil, false, 123, 0xfacep-8, "\001", system.resume(co, '\\0'))
	end)

	assert(system.run() == false)

	done()
end

do case "resume closed"
	local stage
	spawn(function ()
		local co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		stage = 0
		local ok, res = system.resume(co)
		assert(ok == true)
		assert(res == "hello")
		assert(co:close() == true)
		assert(co:status() == "dead")
		asserterr("cannot resume dead coroutine", system.resume(co))
		stage = 1
	end)

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "collect suspended"
	local stage
	spawn(function ()
		garbage.co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		stage = 0
		local ok, res = system.resume(garbage.co)
		assert(ok == true)
		assert(res == "hello")
		gc()
		assert(garbage.co == nil)
		stage = 1
	end)

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "collect canceled"
	local stage
	spawn(function ()
		garbage.coro = coroutine.running()
		garbage.co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		stage = 0
		local ok, res = system.resume(garbage.co)
		assert(ok == nil)
		assert(res == "cancel")
		stage = 1
		assert(preemptco.status(garbage.co) == "running")
	end)

	assert(stage == 0)
	coroutine.resume(garbage.coro, nil, "cancel")
	assert(stage == 1)

	assert(system.run() == false)

	gc()
	assert(garbage.co == nil)

	done()
end

do case "running unreferenced"
	local stage
	spawn(function ()
		stage = 0
		local ok, res = system.resume(preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]])
		assert(ok == true)
		assert(res == "hello")
		stage = 1
	end)

	gc()

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "close running"
	local stage
	local co = preemptco.load[[
		local coroutine = require "coroutine"
		coroutine.yield("hello")
	]]

	spawn(function ()
		stage = 0
		local ok, res = system.resume(co)
		assert(ok == true)
		assert(res == "hello")
		asserterr("cannot resume dead coroutine", system.resume(co))
		stage = 1
	end)

	assert(co:close() == true)
	assert(co:status() == "dead")

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "yield values"
	local stage = 0
	local a,b,c = spawn(function ()
		local co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		local res, extra = system.resume(co, "testing", 1, 2, 3)
		assert(res == true)
		assert(extra == "hello")
		stage = 1
	end)
	assert(a == nil)
	assert(b == nil)
	assert(c == nil)

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		local co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		system.resume(co)
		stage = 1
		coroutine.yield()
		stage = 2
	end)

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "resume same coroutine"
	local co = preemptco.load[[
		local coroutine = require "coroutine"
		coroutine.yield("hello")
	]]

	local stage = 0
	spawn(function ()
		system.resume(co)
		assert(co:status() == "suspended")
		stage = 1
		system.resume(co)
		assert(co:status() == "dead")
		stage = 2
	end)

	assert(stage == 0)
	assert(co:status() == "running")
	assert(system.run("step") == true)
	assert(co:status() == "running")
	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2)
	assert(co:status() == "dead")

	done()
end

do case "resume different coroutines"
	local co1 = preemptco.load[[
		local coroutine = require "coroutine"
		coroutine.yield("hello once")
	]]
	local co2 = preemptco.load[[
		local coroutine = require "coroutine"
		coroutine.yield("hello twice")
	]]

	local stage = 0
	spawn(function ()
		system.resume(co1)
		assert(co1:status() == "suspended")
		assert(co2:status() == "normal")
		stage = 1
		system.resume(co2)
		assert(co1:status() == "suspended")
		assert(co2:status() == "suspended")
		stage = 2
	end)

	assert(stage == 0)
	assert(co1:status() == "running")
	assert(co2:status() == "normal")
	assert(system.run("step") == true)
	assert(co1:status() == "suspended")
	assert(co2:status() == "running")
	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2)
	assert(co1:status() == "suspended")
	assert(co2:status() == "suspended")

	done()
end

do case "resume transfer"
	local stage
	local thread
	spawn(function ()
		local co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
			return "bye"
		]]
		stage = 0
		local res, extra = system.resume(co)
		assert(res == true)
		assert(extra == "hello")
		assert(co:status() == "suspended")
		stage = 2
		coroutine.resume(thread, co)
		stage = 4
	end)
	assert(stage == 0)

	spawn(function ()
		thread = coroutine.running()
		stage = 1
		local co = coroutine.yield()
		stage = 3
		local res, extra = system.resume(co)
		assert(res == true)
		assert(extra == "bye")
		assert(co:status() == "dead")
		stage = 5
	end)
	assert(stage == 1)

	assert(system.run("step") == true)
	assert(stage == 4)
	assert(system.run() == false)
	assert(stage == 5)
	thread = nil

	done()
end

do case "cancel resume"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local co = preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]
		local a,b,c = system.resume(co)
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
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "cancel and resume again"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local co = preemptco.load[[
			require "_G"
			local coroutine = require "coroutine"
			coroutine.yield("hello")
			return "bye"
		]]
		local res, extra = system.resume(co)
		assert(res == nil)
		assert(extra == "cancel")
		assert(co:status() == "running")
		stage = 1
		local res, extra = system.resume(co)
		assert(res == true)
		if extra == "hello" then
			assert(co:status() == "suspended")
		else
			assert(extra == "bye")
			assert(co:status() == "dead")
		end
		stage = 2
	end)

	assert(stage == 0)
	coroutine.resume(garbage.coro, nil, "cancel")
	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "cancel and resume by other"
	local thread
	local stage

	spawn(function ()
		garbage.coro = coroutine.running()
		local co = preemptco.load[[
			require "_G"
			local coroutine = require "coroutine"
			coroutine.yield("hello")
			return "bye"
		]]
		stage = 0
		local res, extra = system.resume(co)
		assert(res == nil)
		assert(extra == "cancel")
		assert(co:status() == "running")
		coroutine.resume(thread, co)
	end)

	assert(stage == 0)

	spawn(function ()
		thread = coroutine.running()
		stage = 1
		local co = coroutine.yield()
		asserterr("cannot resume running coroutine", system.resume(co))
		stage = 2
		while co:status() == "running" do
			system.suspend()
		end
		local status = co:status()
		local res, extra = system.resume(co)
		assert(res == true)
		if status == "normal" then
			assert(extra == "hello")
			assert(co:status() == "suspended")
		else
			assert(status == "suspended")
			assert(extra == "bye")
			assert(co:status() == "dead")
		end
		stage = 3
	end)

	assert(stage == 1)
	coroutine.resume(garbage.coro, nil, "cancel")
	assert(stage == 2)
	assert(system.run() == false)
	assert(stage == 3)

	thread = nil

	done()
end

do case "ignore errors"

	local stage = 0
	pspawn(function ()
		assert(system.resume(preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]))
		stage = 1
		error("oops!")
	end)

	assert(stage == 0)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		assert(system.resume(preemptco.load[[
			local coroutine = require "coroutine"
			coroutine.yield("hello")
		]]))
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro, true)
	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "chunk from file"
	local path = os.tmpname()
	local file = io.open(path, "w")
	file:write[[
		local coroutine = require "coroutine"
		coroutine.yield("hello")
		return "bye"
	]]
	file:close()
	local co = preemptco.loadfile(path)
	os.remove(path)

	local stage
	spawn(function ()
		stage = 0
		local res, extra = system.resume(co)
		assert(res == true)
		assert(extra == "hello")
		assert(co:status() == "suspended")
		stage = 1
		local res, extra = system.resume(co)
		assert(res == true)
		assert(extra == "bye")
		assert(co:status() == "dead")
		stage = 2
	end)
	assert(co:status() == "running")

	assert(stage == 0)
	assert(system.run("step") == true)
	assert(stage == 1)
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "inherit preload"
	spawn(function ()
		package.preload["coutil.spawn"] =
			assert(package.searchers[2]("coutil.spawn"))
		package.preload["coutil.system"] =
			assert(package.searchers[4]("coutil.system"))

		local co = preemptco.load[[
			local package = require "package"
			package.path = ""
			package.cpath = ""

			require "coutil.spawn"
			require "coutil.system"

			return true
		]]

		package.preload["coutil.spawn"] = nil
		package.preload["coutil.system"] = nil

		local ok, res = system.resume(co)
		assert(ok == true and res == true)
	end)

	assert(system.run() == false)

	done()
end

