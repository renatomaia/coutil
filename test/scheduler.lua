local scheduler = require "coutil.scheduler"
local run = scheduler.run
local pause = scheduler.pause
local awaitsig = scheduler.awaitsig

newtest "run" ------------------------------------------------------------------

do case "error messages"
	asserterr("invalid option", pcall(run, "none"))
	asserterr("string expected", pcall(run, coroutine.running()))

	done()
end

do case "empty call"
	assert(scheduler.run() == false)

	done()
end

do case "nested call"
	local stage = 0
	spawn(function (...)
		pause(...)
		asserterr("already running", pcall(run))
		stage = 1
	end)
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "run step|ready"
	for _, mode in ipairs{"step", "ready"} do
		local n = 3
		local stage = {}
		for i = 1, n do
			stage[i] = 0
			spawn(function (c)
				for j = 1, c do
					assert(pause())
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		for i = 1, n do
			gc()
			assert(run(mode) == (i < n))
			for j = 1, n do
				assert(stage[j] == (j < i and j or i))
			end
		end

		gc()
		assert(run() == false)
		for i = 1, n do
			assert(stage[i] == i)
		end
	end

	done()
end

do case "run loop"
	local mode = "loop"
	do ::again::
		local n = 3
		local stage = {}
		for i = 1, n do
			stage[i] = 0
			spawn(function (c)
				for j = 1, c do
					assert(pause())
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		gc()
		assert(run("loop") == false)
		for i = 1, n do
			assert(stage[i] == i)
		end
		if mode == "loop" then
			mode = nil
			goto again
		end
	end

	done()
end

newtest "pause" ----------------------------------------------------------------

do case "error messages"
	asserterr("unable to yield", pcall(pause))

	done()
end

do case "yield values"
	local stage = 0
	local ok, a,b,c,d,e = spawn(function (...)
		assert(pause(...))
		stage = 1
	end, "testing", 1, 2, 3)
	assert(ok == true)
	assert(a == "testing")
	assert(b == 1)
	assert(c == 2)
	assert(d == 3)
	assert(e == nil)
	assert(stage == 0)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		assert(pause())
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule"
	local stage = 0
	spawn(function ()
		assert(pause())
		stage = 1
		assert(pause())
		stage = 2
	end)
	assert(stage == 0)

	gc()
	assert(run("step") == true)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function (...)
		garbage.coro = coroutine.running()
		local ok, a,b,c = pause()
		assert(ok == false)
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
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors"

	local stage = 0
	pspawn(function (errmsg)
		assert(pause())
		stage = 1
		error(errmsg)
	end, "oops!")
	assert(stage == 0)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

newtest "awaitsig" -------------------------------------------------------------

local function sendsignal(name)
	os.execute("killall -"..name.." lua")
end

do case "error messages"
	asserterr("invalid signal", pcall(awaitsig, "kill"))
	asserterr("unable to yield", pcall(awaitsig, "user1"))

	done()
end

do case "yield values"
	local stage = 0
	local ok, a,b,c,d,e = spawn(function (...)
		assert(awaitsig("user1", ...))
		stage = 1
	end, "testing", 1, 2, 3)
	assert(ok == true)
	assert(a == "testing")
	assert(b == 1)
	assert(c == 2)
	assert(d == 3)
	assert(e == nil)
	assert(stage == 0)

	spawn(function (...)
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		assert(awaitsig("user1"))
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function (...)
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule same signal"
	local stage = 0
	spawn(function ()
		assert(awaitsig("user1"))
		stage = 1
		assert(awaitsig("user1"))
		stage = 2
	end)
	assert(stage == 0)

	spawn(function (...)
		pause()
		sendsignal("USR1")
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run("step") == true)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "reschedule different signal"
	local stage = 0
	spawn(function ()
		assert(awaitsig("user1"))
		stage = 1
		assert(awaitsig("user2"))
		stage = 2
	end)
	assert(stage == 0)

	spawn(function (...)
		pause()
		sendsignal("USR1")
		pause()
		sendsignal("USR2")
	end)

	gc()
	assert(run("step") == true)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function (...)
		garbage.coro = coroutine.running()
		local ok, a,b,c = awaitsig("user1")
		assert(ok == false)
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
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors"

	local stage = 0
	pspawn(function ()
		assert(awaitsig("user1"))
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	spawn(function (...)
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end
