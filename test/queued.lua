local event = require "coutil.queued"
local await = event.await
local awaitall = event.awaitall
local awaitany = event.awaitany
local awaiteach = event.awaiteach
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
			assert(select("#", await(e)) == 0)
			a = 1
			assert(select("#", await(e)) == 0)
			a = 2
		end)
		assert(a == 1) -- first 'await' returned, second 'await' suspended

		emitall(e)
		assert(a == 2) -- second 'await' returned

		assert(queued(e) == false)
	end

	done()
end

do case "extra values"
	for _, e in ipairs(types) do
		local v = {}
		emitall(e, v)
		emitall(e, v,e)
		emitall(e, v,e,e)

		local a1, a2, a3
		spawn(function ()
			a1, a2, a3 = await(e)
		end)
		assert(a1 == v)
		assert(a2 == nil)
		assert(a3 == nil)

		local b1, b2, b3
		spawn(function ()
			b1, b2, b3 = await(e)
		end)
		assert(b1 == v)
		assert(b2 == e)
		assert(b3 == nil)

		local c1, c2, c3
		spawn(function ()
			c1, c2, c3 = await(e)
		end)
		assert(c1 == v)
		assert(c2 == e)
		assert(c3 == e)
	end

	for _, e in ipairs(types) do
		local a1, a2, a3
		spawn(function ()
			for i = 1, 3 do
				a1, a2, a3 = await(e)
			end
		end)
		assert(a1 == nil)

		local b1, b2, b3
		spawn(function ()
			for i = 1, 3 do
				b1, b2, b3 = await(e)
			end
		end)
		assert(b1 == nil)

		local c1, c2, c3
		spawn(function ()
			for i = 1, 3 do
				c1, c2, c3 = await(e)
			end
		end)
		assert(c1 == nil)

		local v = {}
		emitall(e, v)
		assert(a1 == v)
		assert(a2 == nil)
		assert(a3 == nil)
		assert(b1 == v)
		assert(b2 == nil)
		assert(b3 == nil)
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

	local a1,a2,a3,a4,a5
	spawn(function ()
		a1 = await(e)
		a2 = emitall(e, t2)
		a3 = await(e)
		a4 = await(e)
	end)

	local b1,b2
	spawn(function ()
		b1 = await(e)
		b2 = emitall(e, t3)
		coroutine.yield()
		b1,b2 = nil
	end)

	emitall(e, t1)
	assert(a1 == t1)
	assert(a2 == false) -- after first 'await', 'b' is not waiting for 'e'
	assert(a3 == t2)
	assert(a4 == t3)
	assert(b1 == t1)
	assert(b2 == true) -- 'emitall' from 'b' resumes 'a'

	emitall(e, {})
	assert(b1 == t1)
	assert(b2 == true)

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
		assert(awaitany(e1,e2,e3) == e1) -- this time this is resumed first
		assert(pending(e1) == false) -- other is awaiting: e1'|e2|e3
		assert(#b == 0)
		assert(emitall(e2) == true) -- resumes other (awaiting: e1|e2|e3)
		assert(#b == 1 and b[1] == 1)
		assert(emitall(e1) == true) -- resumes other (awaiting: e1|e2|e3)
		assert(#b == 2 and b[2] == 2)
		step(a) -- a[1] = 3

		assert(awaitany(e1,e2,e3) == e1) -- this time this is resumed last
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
		assert(awaitany(e1,e2,e3) == e2) -- emitted from the other coroutine
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[1] = 1
		assert(awaitany(e1,e2,e3) == e1) -- emitted from the other coroutine
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		step(b) -- b[2] = 2

		assert(awaitany(e1,e2,e3) == e1) -- this time this is resumed first
		assert(pending(e1) == false) -- other is awaiting: e1'|e2|e3
		step(b) -- b[3] = 4
		assert(awaitany(e1,e2,e3) == e1)
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

newtest "awaiteach" ------------------------------------------------------------

local function newcb()
	local seq = {n=0}
	return seq, function (e, t1,t2,t3)
		seq.n = seq.n+1
		seq[seq.n] = { event=e, t1,t2,t3 }
	end
end

local function assertseq(seq, t, ...)
	local count = select("#", ...)
	assert(seq.n == count)
	for index = 1, count do
		local e = select(index, ...)
		assert(seq[index].event == e)
		assert(seq[index][1] == t[e][1])
		assert(seq[index][2] == t[e][2])
		assert(seq[index][3] == t[e][3])
	end
end

do case "error messages"
	asserterr("unable to yield", pcall(awaiteach, print, 1,2,3))
	assert(pending(1) == false)
	assert(pending(2) == false)
	assert(pending(3) == false)

	local a
	spawn(function ()
		asserterr("attempt to call a nil value", pcall(awaiteach, nil, 1,2,3))
		a = 1
	end)
	assert(emitall(1) == true)
	assert(a == 1)
	assert(pending(2) == false)
	assert(pending(3) == false)

	done()
end

do case "garbage collection"
	garbage.f = function () end
	garbage.e1,garbage.e2,garbage.e3 = {},{},{}

	spawn(awaiteach, f, garbage.e1)
	spawn(awaiteach, f, garbage.e1,garbage.e2)
	spawn(awaiteach, f, garbage.e1,garbage.e2,garbage.e3)

	done()
end

do case "value types"
	local seq, cb = newcb()
	for i, e in ipairs(types) do
		emitall(e)
	end
	local v = {}
	local a = 0
	spawn(function ()
		awaiteach(cb, table.unpack(types))
		a = 1
		awaiteach(cb, table.unpack(types))
		a = 2
		awaiteach(cb, v, table.unpack(types))
		a = 3
	end)

	assert(a == 1)
	assert(seq.n == #types)
	for i, e in ipairs(types) do
		assert(seq[i].event == e)
		assert(pending(e) == true)
		seq[i] = nil
	end
	seq.n = 0

	assert(emitall(v) == false)

	for i, e in ipairs(types) do
		assert(a == 1)
		assert(seq.n == i-1)
		assert(seq[i] == nil)
		assert(emitall(e) == true)
		assert(seq[i].event == e)
	end
	assert(a == 2)
	assert(seq.n == #types+1)
	assert(seq[seq.n].event == v)
	assert(pending(v) == false)

	for i, e in ipairs(types) do
		i = i+#types+1
		assert(a == 2)
		assert(seq[i] == nil)
		assert(emitall(e) == true)
		assert(seq[i].event == e)
	end
	assert(a == 3)
	assert(seq.n == 2*#types+1)

	assert(pending(v) == false)
	for _, e in ipairs(types) do
		assert(pending(e) == false)
	end

	done()
end

do case "ignore duplications"
	local t = {[0]={},{},{},{}}
	local seq, cb = newcb()

	emitall(1)
	emitall(2)
	emitall(3)

	local a = 0
	spawn(function ()
		awaiteach(cb, 1,2,3, 1,2)
		a = 1
		awaiteach(cb, 1,2,3, 2,3)
		a = 2
		awaiteach(cb, 0, 1,2,3, 1,3)
		a = 3
	end)
	assert(a == 1)
	assertseq(seq, t, 1,2,3)
	emitall(0)
	assert(a == 1)
	assertseq(seq, t, 1,2,3)
	emitall(1)
	assert(a == 1)
	assertseq(seq, t, 1,2,3, 1)
	emitall(2)
	assert(a == 1)
	assertseq(seq, t, 1,2,3, 1,2)
	emitall(3)
	assert(a == 2)
	assertseq(seq, t, 1,2,3, 1,2,3, 0)
	emitall(1)
	assert(a == 2)
	assertseq(seq, t, 1,2,3, 1,2,3, 0, 1)
	emitall(2)
	assert(a == 2)
	assertseq(seq, t, 1,2,3, 1,2,3, 0, 1,2)
	emitall(3)
	assert(a == 3)
	assertseq(seq, t, 1,2,3, 1,2,3, 0, 1,2,3)

	done()
end

do case "ignore 'nil's"
	local t = {{},{},{}}
	local seq, cb = newcb()

	local a
	spawn(function ()
		awaiteach(cb, nil)
		a = 1
		awaiteach(cb, 1,2,nil,3)
		a = 2
		awaiteach(cb, 1,2,3,nil)
		a = 3
		awaiteach(cb, nil,1,2,3)
		a = 4
		awaiteach(cb, nil,1,nil,2,3,nil)
		a = 5
	end)
	assert(a == 1)
	for i = 2, 5 do
		seq.n = 0
		seq[1] = nil
		seq[2] = nil
		seq[3] = nil
		assert(emitall(1) == true)
		assertseq(seq, t, 1)
		assert(emitall(2) == true)
		assertseq(seq, t, 1,2)
		assert(emitall(3) == true)
		assertseq(seq, t, 1,2,3)
		assert(a == i)
	end

	assert(pending(1) == false)
	assert(pending(2) == false)
	assert(pending(3) == false)
	assert(pending(nil) == false)
	assertseq(seq, t, 1,2,3)

	done()
end

do case "different sets"
	local e1,e2,e3 = {},{},{}
	local t = {
		[e1] = { {},{},{} },
		[e2] = { {},{},{} },
		[e3] = { {},{},{} },
	}

	local aseq, acb = newcb()
	local bseq, bcb = newcb()

	local a
	spawn(function ()
		a = 1
		awaiteach(acb, e1,e2,e3)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		a = 2
		awaiteach(acb, e1)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		a = 3
		coroutine.yield()
		a = 4
	end)
	assert(a == 1)

	local b
	spawn(function ()
		awaiteach(bcb) -- no effect
		b = 1
		awaiteach(bcb, e2)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		b = 2
		awaiteach(bcb, e1,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		b = 3
		coroutine.yield()
		b = 4
	end)
	assert(b == 1)

	assert(emitall(e1, table.unpack(t[e1])) == true)
	assert(a == 1)
	assert(b == 1)
	assertseq(aseq, t, e1)
	assertseq(bseq, t)
	assert(pending(e1, {},{},{}) == false)
	assert(a == 1)
	assert(b == 1)
	assertseq(aseq, t, e1)
	assertseq(bseq, t)

	assert(emitall(e2, table.unpack(t[e2])) == true)
	assert(a == 1)
	assert(b == 2)
	assertseq(aseq, t, e1,e2)
	assertseq(bseq, t, e2)
	assert(pending(e2, {},{},{}) == false)
	assert(a == 1)
	assert(b == 2)
	assertseq(aseq, t, e1,e2)
	assertseq(bseq, t, e2)

	assert(emitall(e3, table.unpack(t[e3])) == true)
	assert(a == 2)
	assert(b == 2)
	assertseq(aseq, t, e1,e2,e3)
	assertseq(bseq, t, e2,e3)
	assert(pending(e3, {},{},{}) == false)
	assert(a == 2)
	assert(b == 2)
	assertseq(aseq, t, e1,e2,e3)
	assertseq(bseq, t, e2,e3)

	assert(emitall(e1, table.unpack(t[e1])) == true)
	assert(a == 3)
	assert(b == 3)
	assertseq(aseq, t, e1,e2,e3,e1)
	assertseq(bseq, t, e2,e3,e1)
	assert(pending(e1, {},{},{}) == false)
	assert(a == 3)
	assert(b == 3)
	assertseq(aseq, t, e1,e2,e3,e1)
	assertseq(bseq, t, e2,e3,e1)
	assert(pending(e2, {},{},{}) == false)
	assert(a == 3)
	assert(b == 3)
	assertseq(aseq, t, e1,e2,e3,e1)
	assertseq(bseq, t, e2,e3,e1)
	assert(pending(e3, {},{},{}) == false)
	assert(a == 3)
	assert(b == 3)
	assertseq(aseq, t, e1,e2,e3,e1)
	assertseq(bseq, t, e2,e3,e1)

	done()
end

do case "resume order"
	local e1,e2,e3 = {},{},{}
	local t = {
		[e1] = {},
		[e2] = {},
		[e3] = {},
	}
	local es = { e1,e2,e3 }
	local order = counter()
	local max = 15

	local list = {}
	local seqof = {}
	for i = 1, max do
		local cb
		seqof[i], cb = newcb()
		spawn(function ()
			awaiteach(cb, table.unpack(es, 1, 1+(i-1)%3))
			list[i] = order()
		end)
		assert(list[i] == nil)
	end

	assert(emitall(e3) == true)
	assert(#list == 0)
	for i = 3, max, 3 do
		assertseq(seqof[i], t, e3)
	end
	assert(emitall(es[2]) == true)
	assert(#list == 0)
	for i = 2, max, 3 do
		assertseq(seqof[i+0], t, e2)
		assertseq(seqof[i+1], t, e3,e2)
	end
	assert(emitall(es[1]) == true)
	assert(#list == max)
	for i = 1, max, 3 do
		assertseq(seqof[i+0], t, e1)
		assertseq(seqof[i+1], t, e2,e1)
		assertseq(seqof[i+2], t, e3,e2,e1)
	end
	for i, order in ipairs(list) do
		assert(i == order)
	end

	done()
end

do case "nested emission"
	local e1,e2,e3 = {},{},{}
	local t = {
		[e1] = {},
		[e2] = {},
		[e3] = {},
	}

	local aseq, acb = newcb()
	local bseq, bcb = newcb()

	local order = counter()
	local function step(list) list[#list+1] = order() end
	local a = {}
	local b = {}

	spawn(function ()
		awaiteach(acb, e1,e2,e3)
		assertseq(aseq, t, e1,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		assertseq(aseq, t, e1,e2,e3)
		step(a) -- a[1] = 1

		awaiteach(acb, e1,e2,e3)
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(emitall(e3) == true) -- emits to other (awaiting: e2')
		assertseq(bseq, t, e1,e3)
		assert(#b == 1 and b[1] == 2) -- other was not resumed
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2) -- callback was not called
		step(a) -- a[2] = 3

		awaiteach(acb, e1,e2,e3)
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2,e1,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2,e1,e2,e3) -- callback was not called

		step(a) -- a[3] = 5
		coroutine.yield()
		step(a)
	end)

	spawn(function ()
		awaiteach(bcb, e1)
		assertseq(aseq, t, e1)
		assertseq(bseq, t, e1)
		assert(pending(e1) == false)
		assert(emitall(e2) == true) -- emits to other (awaiting: e3)
		assertseq(aseq, t, e1,e2)
		assert(#a == 0) -- other was not resumed
		assert(emitall(e3) == true) -- resumes other (awaiting: e1,e2,e3)
		assert(#a == 1 and a[1] == 1) -- other was not resumed
		assert(emitall(e1) == true) -- emits to other (awaiting: e2,e3)
		assertseq(aseq, t, e1,e2,e3,e1)
		assert(emitall(e3) == true) -- emits to other (awaiting: e2)
		assertseq(aseq, t, e1,e2,e3,e1,e3)
		assert(#a == 1 and a[1] == 1) -- other was not resumed
		assertseq(bseq, t, e1) -- callback was not called
		step(b) -- b[1] = 2

		awaiteach(bcb, e2,e3)
		assertseq(bseq, t, e1,e3,e2)
		assert(emitall(e1) == true) -- emits to other (awaiting: e2,e3)
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2,e1)
		assert(emitall(e2) == true) -- emits to other (awaiting: e3)
		assertseq(aseq, t, e1,e2,e3,e1,e3,e2,e1,e2)
		assert(#a == 2 and a[2] == 3) -- other was not resumed
		assertseq(bseq, t, e1,e3,e2) -- callback was not called
		step(b) -- b[2] = 4

		awaiteach(bcb, e3)
		assertseq(bseq, t, e1,e3,e2,e3)
		assert(pending(e1) == false)
		assert(pending(e2) == false)
		assert(pending(e3) == false)
		assertseq(bseq, t, e1,e3,e2,e3) -- callback was not called
		step(b) -- b[3] = 6

		coroutine.yield()
		step(b)
	end)

	assert(emitall(e1) == true)
	assert(#a == 1 and a[1] == 1) -- awaiting: e2
	assert(#b == 1 and b[1] == 2) -- awaiting: e2,e3

	assert(emitall(e2) == true)
	assert(#a == 2 and a[2] == 3) -- awaiting: e3
	assert(#b == 2 and b[2] == 4) -- awaiting: e3

	assert(emitall(e3) == true)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)

	assert(pending(e1) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)
	assert(pending(e2) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)
	assert(pending(e3) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)
	assertseq(aseq, t, e1,e2,e3,e1,e3,e2,e1,e2,e3) -- callback was not called
	assertseq(bseq, t, e1,e3,e2,e3) -- callback was not called

	done()
end

do case "cancelation"
	emitall(1)
	local a = 0
	spawn(function ()
		local function oops() error("oops!") end
		asserterr("oops!", pcall(awaiteach, oops, 1,2,3))
		a = 1
		asserterr("oops!", pcall(awaiteach, oops, 1,2,3))
		a = 2
	end)
	assert(a == 1)
	emitall(2)
	assert(a == 2)
	assert(pending(1) == false)
	assert(pending(2) == false)
	assert(pending(3) == false)

	emitall(2)
	local a = 0
	spawn(function ()
		local t1,t2,t3 = {},{},{}
		local function is2(e)
			if e == 2 then
				return t1,t2,t3
			end
		end
		local r1,r2,r3 = awaiteach(is2, 1,2,3)
		assert(r1 == t1)
		assert(r2 == t2)
		assert(r3 == t3)
		a = 1
		local r1,r2,r3 = awaiteach(is2, 1,2,3)
		assert(r1 == t1)
		assert(r2 == t2)
		assert(r3 == t3)
		a = 2
	end)
	assert(a == 1)
	emitall(2)
	assert(a == 2)
	assert(pending(1) == false)
	assert(pending(2) == false)
	assert(pending(3) == false)

	done()
end
