local event = require "coutil.event"
local await = event.await
local emitall = event.emitall

local mutex = require "coutil.mutex"
local lock = mutex.lock
local unlock = mutex.unlock
local islocked = mutex.islocked
local ownlock = mutex.ownlock

newtest "mutex" ----------------------------------------------------------------

do case "garbage collection"
	garbage.m = {}
	lock(garbage.m)
	assert(unlock(garbage.m) == false)

	done()
end

do case "lock nil"
	asserterr("table index is nil", pcall(lock))
	asserterr("table index is nil", pcall(lock, nil))

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		lock(e)
		assert(islocked(e) == true)
		assert(ownlock(e) == true)
		local locked = true

		local a = 0
		spawn(function ()
			lock(e)
			assert(locked == false)
			assert(ownlock(e) == true)
			assert(unlock(e) == false)
			assert(ownlock(e) == false)
			a = 1
		end)
		assert(a == 0) -- lock suspended the coroutine

		locked = false
		assert(unlock(e) == true)
		assert(a == 1) -- coroutine finally executed
		assert(islocked(e) == false)
		assert(ownlock(e) == false)
	end

	done()
end

do case "lock queue"
	local e = {}
	lock(e)
	local locked = true

	local a = {}
	local n = 10
	for i = 1, n do
		spawn(function ()
			lock(e)
			assert(locked == false)
			a[i] = 1
			for j = 1, n do
				assert(a[j] == (j <= i and 1 or nil)) -- entered in order
			end
			assert(unlock(e) == (i < n))
			a[i] = 2
		end)
		assert(a[i] == nil) -- lock suspended the coroutine
	end

	locked = false
	assert(unlock(e) == true)
	for i = 1, n do
		assert(a[i] == 2) -- all threads compled execution
	end

	done()
end

do case "not owned"
	local e = {}

	local a = 0
	spawn(function ()
		asserterr("lock not owned", pcall(unlock, e))
		assert(islocked(e) == false)
		assert(ownlock(e) == false)
		a = 1
	end)
	assert(a == 1)

	asserterr("lock not owned", pcall(unlock, e))
	assert(islocked(e) == false)
	assert(ownlock(e) == false)

	done()
end

do case "nesting"
	local e = {}
	lock(e)
	local locked = true

	local a = 0
	spawn(function ()
		lock(e)
		asserterr("nested lock", pcall(lock, e))
		assert(unlock(e) == false)
		a = 1
	end)
	assert(a == 0) -- lock suspended the coroutine

	asserterr("nested lock", pcall(lock, e))
	assert(a == 0) -- still haven't executed

	locked = false
	assert(unlock(e) == true)
	assert(a == 1) -- coroutine finally executed

	done()
end

do case "fake unlock"
	local e = {}
	lock(e)
	local locked = true

	local a = {}
	local n = 10
	for i = 1, n do
		spawn(function ()
			lock(e)
			assert(locked == false)
			a[i] = 1
			for j = 1, n do
				assert(a[j] == (j <= i and 1 or nil)) -- entered in order
			end
			assert(unlock(e) == (i < n))
			a[i] = 2
		end)
		assert(a[i] == nil) -- lock suspended the coroutine
	end

	assert(emitall(e) == true)
	for i = 1, n do
		assert(a[i] == nil) -- still waiting lock
	end

	locked = false
	assert(unlock(e) == true)
	for i = 1, n do
		assert(a[i] == 2) -- coroutine finally executed
	end

	done()
end

do case "unlock event"
	local e = {}
	lock(e)
	local locked = true

	local a = 0
	spawn(function ()
		await(e)
		a = 1
		lock(e)
		a = 2
		assert(locked == false)
		assert(unlock(e) == false)
		a = 3
	end)
	assert(a == 0) -- await suspended the coroutine

	locked = false
	assert(unlock(e) == true)
	assert(a == 3) -- coroutine finally executed

	done()
end

do case "ignored unlock"
	local e = {}
	lock(e)
	local locked = true

	local a = 0
	spawn(function ()
		await(e)
		a = 1
	end)
	assert(a == 0) -- await suspended the coroutine

	local b = 0
	spawn(function ()
		lock(e)
		b = 1
		assert(locked == false)
		assert(unlock(e) == false)
		b = 2
	end)
	assert(a == 0) -- await suspended the coroutine

	locked = false
	assert(unlock(e) == true)

	assert(a == 1) -- 'a' completed
	assert(b == 0) -- 'b' still waiting lock
	assert(islocked(e) == false)

	assert(emitall(e) == true)
	assert(b == 2) -- 'b' finally completed

	done()
end
