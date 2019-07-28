local system = require "coutil.system"

newtest "isrunning" ------------------------------------------------------------------

do case "before and after run"
	assert(system.isrunning() == false)

	spawn(function ()
		assert(system.isrunning() == false)
		system.suspend()
		assert(system.isrunning() == true)
	end)

	assert(system.isrunning() == false)
	assert(system.run() == false)
	assert(system.isrunning() == false)

	done()
end

do case "before and after halt"
	spawn(function ()
		assert(system.isrunning() == false)
		system.suspend()
		assert(system.isrunning() == true)
		system.halt()
		system.suspend()
		assert(system.isrunning() == true)
	end)

	assert(system.isrunning() == false)
	assert(system.run() == true)
	assert(system.isrunning() == false)
	assert(system.run() == false)
	assert(system.isrunning() == false)

	done()
end

newtest "run" ------------------------------------------------------------------

do case "error messages"
	asserterr("invalid option", pcall(system.run, "none"))
	asserterr("string expected", pcall(system.run, coroutine.running()))

	done()
end

do case "empty call"
	assert(system.run() == false)

	done()
end

do case "nested call"
	local stage = 0
	spawn(function ()
		system.suspend()
		asserterr("already running", pcall(system.run))
		stage = 1
	end)
	assert(system.run() == false)
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
					system.suspend()
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		for i = 1, n do
			gc()
			assert(system.run(mode) == (i < n))
			for j = 1, n do
				assert(stage[j] == (j < i and j or i))
			end
		end

		gc()
		assert(system.run() == false)
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
					system.suspend()
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		gc()
		assert(system.run("loop") == false)
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

newtest "halt" -----------------------------------------------------------------

do case "error messages"
	asserterr("not running", pcall(system.halt))

	done()
end

do case "halt loop"
	spawn(function ()
		garbage.thread = coroutine.running()
		system.suspend(1000)
	end)
	spawn(function ()
		system.suspend()
		system.halt()
	end)
	assert(system.run() == true)

	coroutine.resume(garbage.thread)

	assert(system.run() == false)

	done()
end
