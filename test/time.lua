local system = require "coutil.system"

newtest "time" -----------------------------------------------------------------

do case "cached time"
	local function testtime()
		local factor = 1e3 -- to milliseconds

		local cached = system.time("updated")*factor
		assert(cached > 0, cached)

		local actual = system.nanosecs()
		repeat until system.nanosecs() > actual+2e6

		assert(cached == system.time()*factor)
		local updated = system.time("updated")*factor
		assert(updated-cached >= 1, updated-cached)

		local epoch, luatime = system.time("epoch"), os.time()
		assert(math.abs(epoch - luatime) < 1)
	end

	testtime()

	spawn(function ()
		system.suspend()
		testtime()
	end)
	assert(system.run() == false)

	done()
end

do case "nanosecs"
	local before = system.nanosecs()
	local elapsed = system.nanosecs()-before
	assert(elapsed > 0, elapsed)
	assert(elapsed < 1e3, elapsed)

	spawn(function ()
		before = system.nanosecs()
		system.time("updated")
		system.suspend(1e-3)
		elapsed = system.nanosecs()-before
	end)

	assert(system.run() == false)
	assert(elapsed >= 1e6, elapsed)
	assert(elapsed < 1e9, elapsed)

	done()
end

newtest "suspend" --------------------------------------------------------------

do case "error messages"
	asserterr("number expected", pcall(system.suspend, false))
	asserterr("unable to yield", pcall(system.suspend))
	asserterr("out of range", pcall(system.suspend, math.maxinteger, "~"))
	asserterr("out of range", pcall(system.suspend, 0x1p64/999.9))

	done()
end

do case "block main thread"
	local before = system.nanosecs()
	system.suspend(.1, "~")
	assert(system.nanosecs()-before > 1e8)

	system.suspend(0, "~")
	system.suspend(-1, "~")
	system.suspend(nil, "~")
	assert(system.nanosecs()-before < 1e9)

	done()
end

do case "block other coroutines"
	local a,b

	spawn(function ()
		a = 0
		system.suspend(0)
		assert(b == 1)
		a = 1
		system.suspend(.1, "~")
		a = 2
	end)
	assert(a == 0)

	spawn(function ()
		b = 0
		system.suspend(0)
		assert(a == 0)
		local before = system.nanosecs()
		b = 1
		system.suspend(0)
		assert(a == 2)
		assert(system.nanosecs()-before > 1e8)
		b = 2
	end)
	assert(b == 0)

	local before = system.nanosecs()
	assert(system.run() == false)
	assert(system.nanosecs()-before > 1e8)

	assert(a == 2)
	assert(b == 2)

	done()
end

local args = { nil, -1, 0, 0.1 }

do case "yield values"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		local a,b,c = spawn(function ()
			local res, extra = system.suspend(delay, nil, "testing", 1, 2, 3)
			assert(res == true)
			assert(extra == nil)
			stage = 1
		end)
		assert(a == nil)
		assert(b == nil)
		assert(c == nil)
		assert(stage == 0)
		gc()

		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "scheduled yield"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			system.suspend(delay)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)
		gc()

		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			local res, extra = system.suspend(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 1
			local res, extra = system.suspend(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 2
		end)
		assert(stage == 0)

		gc()
		assert(system.run("step") == true)
		assert(stage == 1)

		gc()
		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		gc()
		assert(system.run() == false)
		assert(stage == 2)
	end

	done()
end

do case "cancel schedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local a,b,c = system.suspend(delay)
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
	end

	done()
end

do case "cancel and reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local extra = system.suspend(delay)
			assert(extra == nil)
			stage = 1
			assert(system.suspend(delay) == true)
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)
	end

	done()
end

do case "resume while closing"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			assert(system.suspend(delay) == nil)
			stage = 1
			local a,b,c = system.suspend(delay)
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
	end

	done()
end

do case "ignore errors"

	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		pspawn(function (errmsg)
			system.suspend(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "ignore errors after cancel"

	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		pspawn(function (errmsg)
			garbage.coro = coroutine.running()
			system.suspend(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
	end


	done()
end

do case "terminate scheduled"
	dostring(utilschunk..[[
		local system = require "coutil.system"
		spawn(function () system.suspend() end)
	]])
	done()
end
