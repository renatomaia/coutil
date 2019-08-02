local event = require "coutil.event"
local await = event.await
local emitall = event.emitall

local promise = require "coutil.promise"
--local onlypending = promise.onlypending
--local pickready = promise.pickready
--local awaitall = promise.awaitall
--local awaitany = promise.awaitany
local newpromise = promise.create

newtest "promise" --------------------------------------------------------------

do case "garbage collection"
	garbage.p1, garbage.f1 = newpromise()
	garbage.p2, garbage.f2 = newpromise()
	garbage.f2(1,2,3)

	done()
end

do case "empty results"
	local p, f = newpromise()
	assert(p("probe") == false)

	f()
	assert(p("probe") == true)

	assert(select("#", p()) == 0)

	done()
end

do case "result storage"
	for _, v in ipairs(types) do
		local p, f = newpromise()

		f(v,v,v)

		local r1,r2,r3 = p()
		assert(r1 == v)
		assert(r2 == v)
		assert(r3 == v)
	end

	done()
end

do case "result replacement"
	local p, f = newpromise()

	for i = 1, 3 do
		local t1,t2,t3 = {},{},{}
		assert(f(t1,t2,t3) == (i==1))

		local r1,r2,r3 = p()
		assert(r1 == t1)
		assert(r2 == t2)
		assert(r3 == t3)
	end

	done()
end

do case "coroutine suspension"
	local p, f = newpromise()

	local t1,t2,t3 = {},{},{}

	local a,a1,a2,a3 = 0
	spawn(function ()
		a1,a2,a3 = p()
		assert(p("probe") == true)
		a = 1
	end)
	assert(a == 0) -- 'p' suspended the coroutine

	f(t1,t2,t3)
	assert(a == 1) -- await returned
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)

	local r1,r2,r3 = p()
	assert(r1 == t1)
	assert(r2 == t2)
	assert(r3 == t3)

	done()
end

do case "fake fulfillment"
	local p, f = newpromise()

	for _ = 1, 3 do
		local t1,t2,t3 = {},{},{}

		local a,a1,a2,a3 = 0
		spawn(function ()
			a1,a2,a3 = p()
			assert(p("probe") == false)
			a = 1
		end)
		assert(a == 0) -- 'p' suspended the coroutine

		assert(emitall(p, t1,t2,t3) == true)
		assert(a == 1) -- await returned
		assert(a1 == t1)
		assert(a2 == t2)
		assert(a3 == t3)
	end

	done()
end

do case "canceled await"
	local p, f = newpromise()

	for _ = 1, 3 do
		local t1,t2,t3 = {},{},{}

		local a,a1,a2,a3 = 0
		local t
		spawn(function ()
			t = coroutine.running()
			a1,a2,a3 = p()
			assert(p("probe") == false)
			a = 1
		end)
		assert(a == 0) -- 'p' suspended the coroutine

		assert(coroutine.resume(t, t1,t2,t3) == true)
		assert(a == 1) -- await returned
		assert(a1 == t1)
		assert(a2 == t2)
		assert(a3 == t3)
	end

	done()
end

do case "fulfillment event"
	local p, f = newpromise()

	local t1,t2,t3 = {},{},{}

	local a,a1,a2,a3 = 0
	spawn(function ()
		_,a1,a2,a3 = await(p)
		assert(p("probe") == true)
		a = 1
	end)
	assert(a == 0) -- 'p' suspended the coroutine

	assert(f(t1,t2,t3) == true)
	assert(a == 1) -- await returned
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)

	done()
end

do case "missed event"
	local p, f = newpromise()

	f()

	local a = 0
	spawn(function ()
		await(p)
		a = 1
	end)
	assert(a == 0) -- 'p' suspended the coroutine

	f(1,2,3)
	assert(a == 0) -- still suspended

	emitall(p)
	assert(a == 1) -- await returned

	done()
end
