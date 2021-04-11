local system = require "coutil.system"

newtest "awaitsig" -------------------------------------------------------------

local function sendsignal(signal)
	system.emitsig(system.procinfo("#"), signal)
end

do case "error messages"
	asserterr("unable to yield", pcall(system.awaitsig, "userdef1"))
	asserterr("unable to yield", pcall(system.awaitsig, "kill"))
	asserterr("invalid signal", pspawn(system.awaitsig, "kill"))

	done()
end

do case "yield values"
	local stage = 0
	local a,b,c = spawn(function ()
		local res, extra = system.awaitsig("userdef1", "testing", 1, 2, 3)
		assert(res == "userdef1")
		assert(extra == nil)
		stage = 1
	end)
	assert(a == nil)
	assert(b == nil)
	assert(c == nil)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		sendsignal("userdef1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		system.awaitsig("userdef1")
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		sendsignal("userdef1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule same signal"
	local stage = 0
	spawn(function ()
		system.awaitsig("userdef1")
		stage = 1
		system.awaitsig("userdef1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		sendsignal("userdef1")
		system.suspend()
		sendsignal("userdef1")
	end)

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "reschedule different signal"
	local stage = 0
	spawn(function ()
		system.awaitsig("userdef1")
		stage = 1
		system.awaitsig("userdef2")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		sendsignal("userdef1")
		system.suspend()
		sendsignal("userdef2")
	end)

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
		local a,b,c = system.awaitsig("userdef1")
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
		assert(system.awaitsig("userdef1") == nil)
		stage = 1
		assert(system.awaitsig("userdef1") == "userdef1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend() -- the first signal handle is active.
		system.suspend() -- the first signal handle is being closed.
		sendsignal("userdef1") -- the second signal handle is active.
	end)

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
		assert(system.awaitsig("userdef1") == nil)
		stage = 1
		local a,b,c = system.awaitsig("userdef1")
		assert(a == .1)
		assert(b == 2.2)
		assert(c == 33.3)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
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
		assert(system.awaitsig("userdef1"))
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	spawn(function ()
		system.suspend()
		sendsignal("userdef1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		system.awaitsig("userdef1")
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
