local event = require "coutil.queued"
local await = event.await
local awaitall = event.awaitall
local awaitany = event.awaitany
local emitall = event.emitall
local emitone = event.emitone
local pending = event.pending
local queued = event.queued

newtest "pending" --------------------------------------------------------------

do case "garbage collection"
	garbage.e = {}
	assert(pending(garbage.e) == false)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		emitall(e)
		assert(pending(e) == false)
		spawn(await, e)
		assert(pending(e) == false)
		spawn(await, e)
		assert(pending(e) == true)
		emitall(e)
		assert(pending(e) == false)

		assert(queued(e) == false)
	end

	assert(pending() == false)
	assert(pending(nil) == false)

	done()
end

newtest "queued" -----------------------------------------------------------------

do case "garbage collection"
	garbage.e = {}
	assert(queued(garbage.e) == false)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		assert(queued(e) == false)
		emitall(e)
		assert(queued(e) == true)
		spawn(await, e)
		assert(queued(e) == false)
		spawn(await, e)
		assert(queued(e) == false)
		emitall(e)
		assert(queued(e) == false)
	end

	assert(queued() == false)
	assert(queued(nil) == false)

	done()
end

newtest "emitall" --------------------------------------------------------------

do case "garbage collection"
	garbage.e = {}
	emitall(garbage.e)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		assert(emitall(e) == false) -- queue event
		local a = 0
		spawn(function ()
			await(e)
			a = 1
			await(e)
			a = 2
		end)
		assert(a == 1) -- first 'await' consumed event, second 'await' suspended

		assert(emitall(e) == true) -- emit event to be consumed
		assert(a == 2) -- second 'await' returned

		assert(queued(e) == false)
	end

	assert(emitall() == false)
	assert(emitall(nil) == false)

	done()
end

do case "event emission"
	for _, e in ipairs(types) do
		assert(emitall(e) == false) -- queue event
		gc()
		local a = 0
		spawn(function ()
			await(e)
			a = 1
		end)
		assert(a == 1) -- 'await' consumed event

		local count = 3
		local done = {}
		for i = 1, count do
			done[i] = 0
			spawn(function ()
				await(e)
				done[i] = 1
			end)
			assert(done[i] == 0) -- 'await' suspended
		end

		assert(emitall(e) == true) -- emit event to be consumed
		for i = 1, count do
			assert(done[i] == 1) -- 'await' returned for all coroutines
		end

		assert(queued(e) == false)
	end

	done()
end

newtest "emitone" --------------------------------------------------------------

do case "garbage collection"
	garbage.e = {}
	emitone(garbage.e)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		assert(emitone(e) == false) -- queue event
		gc()
		local a = 0
		spawn(function ()
			await(e)
			a = 1
			await(e)
			a = 2
		end)
		assert(a == 1) -- first 'await' consumed event, second 'await' suspended

		assert(emitone(e) == true) -- emit event to be consumed
		assert(a == 2) -- second 'await' returned

		assert(queued(e) == false)
	end

	assert(emitone() == false)
	assert(emitone(nil) == false)

	done()
end

do case "event emission"
	for _, e in ipairs(types) do
		assert(emitone(e) == false) -- queue event
		gc()
		local a = 0
		spawn(function ()
			await(e)
			a = 1
		end)
		assert(a == 1) -- 'await' consumed event

		local count = 3
		local done = {}
		for i = 1, count do
			done[i] = 0
			spawn(function ()
				await(e)
				done[i] = 1
			end)
			assert(done[i] == 0) -- await suspended
		end

		for i = 1, count do
			assert(emitone(e) == true) -- emit event to be consumed
			for j = 1, count do
				assert(done[j] == (j <= i and 1 or 0)) -- earliest 'await's returned
			end
		end

		assert(queued(e) == false)
	end

	done()
end

newtest "await" ----------------------------------------------------------------

do case "error messages"
	asserterr("table index is nil", pspawn(await))
	asserterr("table index is nil", pspawn(await, nil))
	asserterr("unable to yield", pcall(await, "e"))
	assert(pending("e") == false) -- failed 'await' had no effect

	done()
end

do case "garbage collection"
	garbage.e = {}
	spawn(await, garbage.e)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		emitall(e)
		gc()
		local a = 0
		spawn(function ()
			assert(await(e) == e)
			a = 1
			assert(await(e) == e)
			a = 2
		end)
		assert(a == 1) -- first 'await' returned, second 'await' suspended

		emitall(e)
		assert(a == 2) -- second 'await' returned

		assert(queued(e) == false)
	end

	done()
end

do case "direct resume"
	for _, e in ipairs(types) do
		local res, v1,v2,v3
		local a = 0
		spawn(function (...)
			garbage.co = coroutine.running()
			res, v1,v2,v3 = await(e)
			a = 1
			coroutine.yield()
			a = 2
		end)
		assert(a == 0)

		coroutine.resume(garbage.co, garbage,1,2,3)
		assert(a == 1)
		assert(res == garbage)
		assert(v1 == 1)
		assert(v2 == 2)
		assert(v3 == 3)

		assert(pending(e) == false)
	end

	done()
end

do case "extra values"
	for _, e in ipairs(types) do
		local v = {}
		emitall(e, v)
		emitall(e, v,e)
		emitall(e, v,e,e)

		local a1, a2, a3, a4
		spawn(function ()
			a1, a2, a3, a4 = await(e)
		end)
		assert(a1 == e)
		assert(a2 == v)
		assert(a3 == nil)
		assert(a4 == nil)

		local b1, b2, b3, b4
		spawn(function ()
			b1, b2, b3, b4 = await(e)
		end)
		assert(b1 == e)
		assert(b2 == v)
		assert(b3 == e)
		assert(b4 == nil)

		local c1, c2, c3, c4
		spawn(function ()
			c1, c2, c3, c4 = await(e)
		end)
		assert(c1 == e)
		assert(c2 == v)
		assert(c3 == e)
		assert(c4 == e)
	end

	for _, e in ipairs(types) do
		local ae, a1, a2, a3
		spawn(function ()
			for i = 1, 3 do
				ae, a1, a2, a3 = await(e)
			end
		end)
		assert(a1 == nil)

		local be, b1, b2, b3
		spawn(function ()
			for i = 1, 3 do
				be, b1, b2, b3 = await(e)
			end
		end)
		assert(b1 == nil)

		local c1, c2, c3
		spawn(function ()
			for i = 1, 3 do
				ce, c1, c2, c3 = await(e)
			end
		end)
		assert(c1 == nil)

		local v = {}
		emitall(e, v)
		assert(ae == e)
		assert(a1 == v)
		assert(a2 == nil)
		assert(a3 == nil)
		assert(be == e)
		assert(b1 == v)
		assert(b2 == nil)
		assert(b3 == nil)
		assert(ce == e)
		assert(c1 == v)
		assert(c2 == nil)
		assert(c3 == nil)
		emitall(e, v,e)
		assert(a1 == v)
		assert(a2 == e)
		assert(a3 == nil)
		assert(b1 == v)
		assert(b2 == e)
		assert(b3 == nil)
		assert(c1 == v)
		assert(c2 == e)
		assert(c3 == nil)
		emitall(e, v,e,e)
		assert(a1 == v)
		assert(a2 == e)
		assert(a3 == e)
		assert(b1 == v)
		assert(b2 == e)
		assert(b3 == e)
		assert(c1 == v)
		assert(c2 == e)
		assert(c3 == e)
	end

	assert(queued(e) == false)

	done()
end

do case "resume order"
	local e = "event"
	local order = counter()
	local max = 16

	local list = {}
	for i = 1, max do
		spawn(function ()
			await(e)
			list[i] = order()
		end)
		assert(list[i] == nil)
	end

	assert(emitall(e) == true)
	assert(#list == max)
	for i, order in ipairs(list) do
		assert(i == order)
	end

	assert(queued(e) == false)

	done()
end

do case "nested emission"
	local e = "event"
	local t1,t2,t3 = {},{},{}

	local a1,a2,a3,a4,a5,a6,a7
	spawn(function ()
		a1,a2 = await(e)
		a3 = emitall(e, t2)
		a4,a5 = await(e)
		a6,a7 = await(e)
	end)

	local b1,b2,b3
	spawn(function ()
		b1, b2 = await(e)
		b3 = emitall(e, t3)
		coroutine.yield()
		b2,b3 = nil
	end)

	emitall(e, t1)
	assert(a1 == e)
	assert(a2 == t1)
	assert(a3 == false) -- after first 'await', 'b' is not waiting for 'e'
	assert(a4 == e)
	assert(a5 == t2)
	assert(a6 == e)
	assert(a7 == t3)
	assert(b1 == e)
	assert(b2 == t1)
	assert(b3 == true) -- 'emitall' from 'b' resumes 'a'

	emitall(e, {})
	assert(b1 == e)
	assert(b2 == t1)
	assert(b3 == true)

	spawn(await, e)
	assert(queued(e) == false)

	done()
end

newtest "awaitall" -------------------------------------------------------------

do case "error messages"
	asserterr("unable to yield", pcall(awaitall, 1,2,3))
	assert(pending(1) == false)
	assert(pending(2) == false)
	assert(pending(3) == false)

	done()
end

do case "garbage collection"
	garbage.e1,garbage.e2,garbage.e3 = {},{},{}

	spawn(awaitall, garbage.e1)
	spawn(awaitall, garbage.e1,garbage.e2)
	spawn(awaitall, garbage.e1,garbage.e2,garbage.e3)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		emitall(e)
	end
	local v = {}
	local a = 0
	spawn(function ()
		awaitall(table.unpack(types))
		a = 1
		awaitall(table.unpack(types))
		a = 2
		awaitall(v, table.unpack(types))
		a = 3
	end)

	emitall(v)
	for _, e in ipairs(types) do
		assert(a == 1) -- stored events resumed first 'awaitall', but not second
		emitall(e)
	end
	assert(a == 2) -- second 'awaitall' returned after new events emitted

	for _, e in ipairs(types) do
		assert(a == 2) -- second 'awaitall' suspended until new events emitted
		emitall(e)
	end
	assert(a == 3) -- second 'awaitall' finally returned

	assert(queued(t) == false)
	for _, e in ipairs(types) do
		assert(queued(e) == false)
	end

	done()
end

do case "direct resume"
	local a = 0
	spawn(function ()
		garbage.co = coroutine.running()
		local res, v1,v2,v3 = awaitall(table.unpack(types))
		assert(res == garbage)
		assert(v1 == 1)
		assert(v2 == 2)
		assert(v3 == 3)
		a = 1
		coroutine.yield()
		a = 2
	end)

	coroutine.resume(garbage.co, garbage,1,2,3)
	assert(a == 1)

	for _, e in ipairs(types) do
		assert(pending(e) == false)
	end
	assert(a == 1)

	done()
end

do case "resumed after events"
	for _, e in ipairs(types) do
		emitall(e)
	end
	local extra = {}

	local a = 0
	spawn(function ()
		garbage.co = coroutine.running()
		local res, v1,v2,v3 = awaitall(extra, table.unpack(types))
		assert(res == garbage)
		assert(v1 == 1)
		assert(v2 == 2)
		assert(v3 == 3)
		a = 1
		coroutine.yield()
		a = 2
	end)
	assert(a == 0)

	for _, e in ipairs(types) do
		assert(pending(e) == false)
	end
	assert(a == 0)

	coroutine.resume(garbage.co, garbage,1,2,3)
	assert(a == 1)

	assert(pending(extra) == false)
	assert(a == 1)

	done()
end

do case "ignore duplications"
	local t1,t2,t3 = {},{},{}
	emitall(t1)
	emitall(t2)
	emitall(t3)
	emitall(t1)
	emitall(t2)

	local a = 0
	spawn(function ()
		awaitall(t1,t2,t3, t1,t2)
		a = 1
		awaitall(t1,t2,t3, t2,t3)
		a = 2
		awaitall(t1,t2,t3, t1,t3)
		a = 3
	end)
	assert(a == 1) -- stored events resumed first 'awaitall', but not second.

	emitall(t3)
	assert(a == 2) -- new and stored events resumed second 'awaitall'.

	emitall(t1)
	emitall(t2)
	emitall(t3)
	assert(a == 3) -- second 'awaitall' returned after new events emitted.

	assert(queued(t1) == false)
	assert(queued(t2) == false)
	assert(queued(t3) == false)

	done()
end

do case "ignore 'nil's"
	local t1,t2,t3 = {},{},{}
	emitall(t1)
	emitall(t2)
	emitall(t3)

	local a
	spawn(function ()
		awaitall(nil)
		a = 1
		awaitall(t1,t2,nil,t3)
		a = 2
		awaitall(t1,t2,t3,nil)
		a = 3
		awaitall(nil,t1,t2,t3)
		a = 4
		awaitall(nil,t1,nil,t2,t3,nil)
		a = 5
	end)
	assert(a == 2)
	for i = 3, 5 do
		emitall(t1)
		emitall(t2)
		emitall(t3)
		assert(a == i)
	end

	assert(queued(t1) == false)
	assert(queued(t2) == false)
	assert(queued(t3) == false)
	assert(queued(nil) == false)

	done()
end

do case "different sets"
	local e0,e1,e2,e3 = {},{},{},{}
	emitall(e0)
	emitall(e0)
	emitall(e0)
	emitall(e0)

	local a = 0
	spawn(function ()
		awaitall(e0,e1,e2,e3)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		a = 1
		awaitall(e0,e1)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		a = 2
		coroutine.yield()
		a = 3
		a = 4
	end)
	assert(a == 0)

	local b = 0
	spawn(function ()
		awaitall() -- no effect
		b = 1
		awaitall(e0,e2)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		b = 2
		awaitall(e0,e1,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		b = 3
		coroutine.yield()
		b = 4
	end)
	assert(b == 1)

	emitall(e1)
	assert(a == 0)
	assert(b == 1)
	assert(pending(e1) == false)

	emitall(e2)
	assert(a == 0)
	assert(b == 2)
	assert(pending(e2) == false)

	emitall(e3)
	assert(a == 1)
	assert(b == 2)
	assert(pending(e3) == false)

	emitall(e1)
	assert(a == 2)
	assert(b == 3)
	assert(pending(e1) == false)
	assert(pending(e2) == false)
	assert(pending(e3) == false)

	assert(queued(e0) == false)
	assert(queued(e1) == false)
	assert(queued(e2) == false)
	assert(queued(e3) == false)

	done()
end

do case "resume order"
	local v = {}
	local es = { {},{},{} }
	local order = counter()
	local max = 16

	for i = 1, max do
		emitall(v)
	end

	local list = {}
	for i = 1, max do
		spawn(function ()
			awaitall(v, table.unpack(es, 1, 1+(i-1)%3))
			list[i] = order()
		end)
		assert(list[i] == nil)
	end

	emitall(es[3])
	assert(#list == 0)
	emitall(es[2])
	assert(#list == 0)
	emitall(es[1])
	assert(#list == max)
	for i, order in ipairs(list) do
		assert(i == order)
	end

	done()
end

do case "nested emission"
	local e0,e1,e2,e3 = {},{},{},{}
	local order = counter()
	local function step(list) list[#list+1] = order() end
	local a = {}
	local b = {}

	for i = 1, 6 do
		emitall(e0)
	end

	spawn(function ()
		awaitall(e0,e1,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(a) -- a[1] = 1
		awaitall(e0,e1,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		emitall(e3) -- emits to other (awaiting: e2')
		assert(#b == 1 and b[1] == 2)
		step(a) -- a[2] = 3
		awaitall(e0,e1,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(a) -- a[3] = 5
		coroutine.yield()
		step(a)
	end)

	spawn(function ()
		awaitall(e0,e1)
		assert(pending(e1) == false)
		emitall(e2) -- emits to other (awaiting: e3)
		assert(#a == 0)
		emitall(e3) -- resumes other (awaiting: e1,e2,e3)
		assert(#a == 1 and a[1] == 1)
		emitall(e1) -- emits to other (awaiting: e2,e3)
		assert(#a == 1 and a[1] == 1)
		emitall(e3) -- emits to other (awaiting: e2)
		assert(#a == 1 and a[1] == 1)
		step(b) -- b[1] = 2
		awaitall(e0,e2,e3)
		emitall(e1) -- emits to other (awaiting: e2,e3)
		assert(#a == 2 and a[2] == 3)
		emitall(e2) -- emits to other (awaiting: e3)
		assert(#a == 2 and a[2] == 3)
		step(b) -- b[2] = 4
		awaitall(e0,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[3] = 6
		coroutine.yield()
		step(b)
	end)

	emitall(e1)
	assert(#a == 1 and a[1] == 1) -- awaiting: e2
	assert(#b == 1 and b[1] == 2) -- awaiting: e2,e3

	emitall(e2)
	assert(#a == 2 and a[2] == 3) -- awaiting: e3
	assert(#b == 2 and b[2] == 4) -- awaiting: e3

	emitall(e3)
	assert(pending(e1) == false)
	assert(pending(e2) == false)
	assert(pending(e3) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)

	done()
end

newtest "awaitany" -------------------------------------------------------------

do case "error messages"
	asserterr("value expected", pspawn(awaitany))
	asserterr("value expected", pspawn(awaitany, nil))
	asserterr("value expected", pspawn(awaitany, nil,nil))
	asserterr("unable to yield", pcall(awaitany, 1,2,3))
	assert(pending(2) == false)
	assert(pending(3) == false)
	assert(pending(1) == false)

	done()
end

do case "garbage collection"
	garbage.e1,garbage.e2,garbage.e3 = {},{},{}

	spawn(awaitany, garbage.e1)
	spawn(awaitany, garbage.e1,garbage.e2)
	spawn(awaitany, garbage.e1,garbage.e2,garbage.e3)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		emitall(e)
		local a = 0
		spawn(function ()
			awaitany(table.unpack(types))
			a = 1
			awaitany(table.unpack(types))
			a = 2
		end)
		assert(a == 1)

		for _, e in ipairs(types) do
			assert(pending(e) == true)
			assert(a == 1)
		end

		emitall(e)
		assert(a == 2)

		for _, e in ipairs(types) do
			assert(pending(e) == false)
		end
	end

	for _, e in ipairs(types) do
		assert(queued(e) == false)
	end

	done()
end

do case "direct resume"
	for _, e in ipairs(types) do
		local a = 0
		spawn(function ()
			garbage.co = coroutine.running()
			local res, v1,v2,v3 = awaitany(table.unpack(types))
			assert(res == garbage)
			assert(v1 == 1)
			assert(v2 == 2)
			assert(v3 == 3)
			a = 1
			coroutine.yield()
			a = 2
		end)
		assert(a == 0)

		coroutine.resume(garbage.co, garbage,1,2,3)
		assert(a == 1)

		for _, e in ipairs(types) do
			assert(pending(e) == false)
		end
		assert(a == 1)
	end

	done()
end

do case "ignore duplications"
	local e1,e2,e3 = {},{},{}
	emitall(e2)
	local a = 0
	spawn(function ()
		awaitany(e1,e2,e3, e1,e2)
		a = 1
		awaitany(e1,e2,e3, e2,e3)
		a = 2
	end)
	assert(a == 1)
	emitall(e2)
	assert(a == 2)
	assert(pending(e1) == false)
	assert(pending(e2) == false)
	assert(pending(e3) == false)

	assert(queued(e1) == false)
	assert(queued(e2) == false)
	assert(queued(e3) == false)

	done()
end

do case "ignore 'nil's"
	local e1,e2,e3 = {},{},{}
	emitall(e2)
	local a = 0
	spawn(function ()
		awaitany(e1,e2,nil,e3)
		a = 1
		awaitany(e1,e2,e3,nil)
		a = 2
		awaitany(nil,e1,e2,e3)
		a = 3
		awaitany(nil,e1,nil,e2,e3,nil)
		a = 4
	end)
	assert(a == 1)
	for i = 2, 4 do
		emitall(e2)
		assert(a == i)
	end

	assert(pending(e1) == false)
	assert(pending(e2) == false)
	assert(pending(e3) == false)
	assert(pending(nil) == false)

	done()
end

do case "different sets"
	local e0,e1,e2,e3 = {},{},{},{}

	local ae,a1,a2,a3 = e0
	spawn(function ()
		ae,a1,a2,a3 = awaitany(e0,e1,e2,e3)
		ae,a1,a2,a3 = awaitany(e0,e1,e2,e3)
		ae,a1,a2,a3 = awaitany(e0,e1,e2,e3)
		coroutine.yield()
		ae = nil
	end)

	local be,b1,b2,b3 = e0
	spawn(function ()
		be,b1,b2,b3 = awaitany(e1)
		be,b1,b2,b3 = awaitany(e2,e3)
		be,b1,b2,b3 = awaitany(e0,e1,e2)
		coroutine.yield()
		be = nil
	end)

	local t1,t2,t3 = {},{},{}
	assert(emitall(e1, t1,t2,t3) == true)
	assert(ae == e1)
	assert(be == e1)
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)
	assert(b1 == t1)
	assert(b2 == t2)
	assert(b3 == t3)

	local t1,t2,t3 = {},{},{}
	assert(emitall(e2, t1,t2,t3) == true)
	assert(ae == e2)
	assert(be == e2)
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)
	assert(b1 == t1)
	assert(b2 == t2)
	assert(b3 == t3)

	local t1,t2,t3 = {},{},{}
	assert(emitall(e3, t1,t2,t3) == true)
	assert(ae == e3)
	assert(be == e2)
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)

	assert(emitall(e3) == false)
	assert(ae == e3)
	assert(be == e2)

	e0,e1,e2,be = nil
	done()
end

do case "resume order"
	local es = { {},{},{} }
	local max = 16

	local list
	for i = 1, max do
		spawn(function ()
			awaitany(table.unpack(es, 1, 1+(i-1)%3))
			list[#list+1] = i
		end)
	end

	list = {}
	assert(emitall(es[3]) == true)
	assert(#list == max//3)
	for i = 1, #list do assert(list[i] == i*3) end

	list = {}
	assert(emitall(es[2]) == true)
	assert(#list == max//3)
	for i = 1, #list do assert(list[i] == i*3-1) end

	list = {}
	assert(emitall(es[1]) == true)
	assert(#list == max//3+max%3)
	for i = 1, #list do assert(list[i] == i*3-2) end

	assert(emitall(es[1]) == false)
	assert(emitall(es[2]) == false)
	assert(emitall(es[3]) == false)

	done()
end

do case "nested emission"
	local e1,e2,e3 = {},{},{}
	local order = counter()
	local function step(list) list[#list+1] = order() end
	local a = {}
	local b = {}

	spawn(function ()
		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- this time this is resumed first
		assert(pending(e1) == false) -- other is awaiting: e1'|e2|e3
		assert(#b == 0)
		assert(emitall(e2) == true) -- resumes other (awaiting: e1|e2|e3)
		assert(#b == 1 and b[1] == 1)
		assert(emitall(e1) == true) -- resumes other (awaiting: e1|e2|e3)
		assert(#b == 2 and b[2] == 2)
		step(a) -- a[1] = 3

		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- this time this is resumed last
		assert(#b == 3 and b[3] == 4)
		assert(emitall(e1) == true) -- resumes other (awaiting: e1|e2|e3)
		assert(#b == 4 and b[4] == 5)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(a) -- a[2] = 6

		coroutine.yield()
		step(a)
	end)

	spawn(function ()
		local e = awaitany(e1,e2,e3)
		assert(e == e2) -- emitted from the other coroutine
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[1] = 1
		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- emitted from the other coroutine
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[2] = 2

		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- this time this is resumed first
		assert(pending(e1) == false) -- other is awaiting: e1'|e2|e3
		step(b) -- b[3] = 4
		local e = awaitany(e1,e2,e3)
		assert(e == e1)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[4] = 5

		coroutine.yield()
		step(b)
	end)

	assert(emitall(e1) == true)
	assert(#a == 1 and a[1] == 3)
	assert(#b == 2 and b[2] == 2)

	assert(emitall(e1) == true)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)

	assert(pending(e1) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)
	assert(pending(e2) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)
	assert(pending(e3) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)

	done()
end
