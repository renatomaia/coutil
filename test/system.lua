local system = require "coutil.system"
local run = system.run
local pause = system.pause
local awaitsig = system.awaitsig

newtest "run" ------------------------------------------------------------------

do case "error messages"
	asserterr("invalid option", pcall(run, "none"))
	asserterr("string expected", pcall(run, coroutine.running()))

	done()
end

do case "empty call"
	assert(run() == false)

	done()
end

do case "nested call"
	local stage = 0
	spawn(function ()
		pause()
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
					pause()
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
					pause()
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
	asserterr("number expected", pcall(pause, false))
	asserterr("unable to yield", pcall(pause))

	done()
end

local args = { nil, 0, 0.1 }

do case "yield values"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		local a,b,c = spawn(function ()
			local res, extra = pause(delay, "testing", 1, 2, 3)
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
			assert(run("ready") == true)
		else
			assert(run("step") == false)
		end

		assert(run() == false)
		assert(stage == 1)
	end

	done()
end

do case "scheduled yield"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			pause(delay)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)
		gc()

		if delay ~= nil and delay > 0 then
			assert(run("ready") == true)
		else
			assert(run("step") == false)
		end

		assert(run() == false)
		assert(stage == 1)
	end

	done()
end

do case "reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			local res, extra = pause(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 1
			local res, extra = pause(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 2
		end)
		assert(stage == 0)

		gc()
		assert(run("step") == true)
		assert(stage == 1)

		gc()
		if delay ~= nil and delay > 0 then
			assert(run("ready") == true)
		else
			assert(run("step") == false)
		end

		gc()
		assert(run() == false)
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
			local ok, a,b,c = pause(delay)
			assert(ok == nil)
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
	end

	done()
end

do case "cancel and reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local ok, extra = pause(delay)
			assert(ok == nil)
			assert(extra == nil)
			stage = 1
			assert(pause(delay) == true)
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(run() == false)
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
			assert(pause(delay) == nil)
			stage = 1
			local ok, a,b,c = pause(delay)
			assert(ok == nil)
			assert(a == 1)
			assert(b == 22)
			assert(c == 333)
			stage = 2
		end)
		assert(stage == 0)

		spawn(function ()
			pause()
			coroutine.resume(garbage.coro, 1,22,333) -- while being closed.
			assert(stage == 2)
			stage = 3
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(run() == false)
		assert(stage == 3)
	end

	done()
end

do case "ignore errors"

	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		pspawn(function (errmsg)
			pause(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		gc()
		assert(run() == false)
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
			pause(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(run() == false)
		assert(stage == 1)
	end

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
	local a,b,c = spawn(function ()
		local res, extra = awaitsig("user1", "testing", 1, 2, 3)
		assert(res == true)
		assert(extra == nil)
		stage = 1
	end)
	assert(a == nil)
	assert(b == nil)
	assert(c == nil)
	assert(stage == 0)

	spawn(function ()
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
		awaitsig("user1")
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
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
		awaitsig("user1")
		stage = 1
		awaitsig("user1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
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
		awaitsig("user1")
		stage = 1
		awaitsig("user2")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
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
	spawn(function ()
		garbage.coro = coroutine.running()
		local ok, a,b,c = awaitsig("user1")
		assert(ok == nil)
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

do case "cancel and reschedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local ok, extra = awaitsig("user1")
		assert(ok == nil)
		assert(extra == nil)
		stage = 1
		assert(awaitsig("user1") == true)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause() -- the first signal handle is active.
		pause() -- the first signal handle is being closed.
		sendsignal("USR1") -- the second signal handle is active.
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 2)

	done()
end

do case "resume while closing"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		assert(awaitsig("user1") == nil)
		stage = 1
		local ok, a,b,c = awaitsig("user1")
		assert(ok == nil)
		assert(a == .1)
		assert(b == 2.2)
		assert(c == 33.3)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		pause()
		coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
		assert(stage == 2)
		stage = 3
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 3)

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

	spawn(function ()
		pause()
		sendsignal("USR1")
	end)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		awaitsig("user1")
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(run() == false)
	assert(stage == 1)

	done()
end
