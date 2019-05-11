local event = require "coutil.event"
local await = event.await
local awaitall = event.awaitall
local awaitany = event.awaitany
local emitall = event.emitall
local emitone = event.emitone
local pending = event.pending

newtest "pending" --------------------------------------------------------------

do case "garbage collection"
	garbage.e = {}
	assert(pending(garbage.e) == false)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		assert(pending(e) == false)

		local a = 0
		spawn(function ()
			await(e)
			assert(pending(e) == false)
			a = 1
		end)
		assert(a == 0) -- await suspended the coroutine

		assert(pending(e) == true)
		assert(a == 0) -- still haven't executed

		assert(emitall(e) == true)

		assert(pending(e) == false)
	end

	assert(pending() == false)
	assert(pending(nil) == false)

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
		assert(emitall(e) == false)
	end

	assert(emitall() == false)
	assert(emitall(nil) == false)

	done()
end

do case "event emission"
	for _, e in ipairs(types) do
		local count = 3
		local done = {}
		for i = 1, count do
			done[i] = 0
			spawn(function ()
				await(e)
				done[i] = 1
			end)
			assert(done[i] == 0) -- await suspended the coroutine
		end

		assert(emitall(e) == true)
		for i = 1, count do
			assert(done[i] == 1) -- await returned
		end

		assert(emitall(e) == false)
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
		assert(emitone(e) == false)
	end

	assert(emitone() == false)
	assert(emitone(nil) == false)

	done()
end

do case "event emission"
	for _, e in ipairs(types) do
		local count = 3
		local done = {}
		for i = 1, count do
			done[i] = 0
			spawn(function ()
				await(e)
				done[i] = 1
			end)
			assert(done[i] == 0) -- await suspended the coroutine
		end

		for i = 1, count do
			assert(emitone(e) == true)
			for j = 1, count do
				assert(done[j] == (j <= i and 1 or 0))
			end
		end

		assert(emitone(e) == false)
	end

	done()
end

newtest "await" ----------------------------------------------------------------

do case "error messages"
	asserterr("table index is nil", pspawn(await))
	asserterr("table index is nil", pspawn(await, nil))
	asserterr("unable to yield", pcall(await, "e"))
	assert(emitall("e") == false)

	done()
end

do case "garbage collection"
	garbage.e = {}
	spawn(await, garbage.e)

	done()
end

do case "value types"
	for _, e in ipairs(types) do
		local a = 0
		spawn(function ()
			assert(await(e) == e)
			assert(emitall(e) == false)
			a = 1
		end)
		assert(a == 0) -- await suspended the coroutine

		assert(emitall(e) == true)
		assert(a == 1) -- await returned

		assert(emitall(e) == false)
	end

	done()
end

do case "extra values"
	local e = "event"

	local a, a1, a2, a3
	spawn(function ()
		a, a1, a2, a3 = await(e)
	end)
	local b, b1, b2, b3
	spawn(function ()
		b, b1, b2, b3 = await(e)
	end)
	local c, c1, c2, c3
	spawn(function ()
		c, c1, c2, c3 = await(e)
	end)

	local t1,t2,t3 = {},{},{}
	assert(emitall(e, t1,t2,t3) == true)
	assert(a == e)
	assert(a1 == t1)
	assert(a2 == t2)
	assert(a3 == t3)
	assert(b == e)
	assert(b1 == t1)
	assert(b2 == t2)
	assert(b3 == t3)
	assert(c == e)
	assert(c1 == t1)
	assert(c2 == t2)
	assert(c3 == t3)

	done()
end

do case "direct resume"
	local e = "event"

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

	assert(emitall(e) == false)
	assert(a == 1)

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

	done()
end

do case "nested emission"
	local e = "event"
	local t1,t2,t3 = {},{},{}

	local a1,a2,a3,a4,a5
	spawn(function ()
		a1,a2 = await(e)
		a3 = emitall(e, t2)
		a4,a5 = await(e)
	end)

	local b1,b2,b3
	spawn(function ()
		b1,b2 = await(e)
		b3 = emitall(e, t3)
		coroutine.yield()
		b1,b2,b3 = nil
	end)

	assert(emitall(e, t1) == true)
	assert(a1 == e)
	assert(a2 == t1)
	assert(a3 == false) -- emit from first coroutine resumes no coroutines
	assert(a4 == e)
	assert(a5 == t3)
	assert(b1 == e)
	assert(b2 == t1)
	assert(b3 == true) -- emit from second coroutine resumes the first one

	assert(emitall(e, {}) == false)
	assert(b1 == e)
	assert(b2 == t1)
	assert(b3 == true)

	done()
end

newtest "awaitall" -------------------------------------------------------------

do case "error messages"
	asserterr("unable to yield", pcall(awaitall, 1,2,3))
	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)

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
	local a = 0
	spawn(function ()
		assert(awaitall(table.unpack(types)) == true)
		a = 1
	end)

	for _, e in ipairs(types) do
		assert(a == 0)
		assert(pending(e) == true)
		assert(a == 0)
		assert(emitall(e) == true)
	end
	assert(a == 1)

	for _, e in ipairs(types) do
		assert(emitall(e) == false)
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
		assert(a == 1)
		assert(pending(e) == false)
		assert(a == 1)
		assert(emitall(e) == false)
	end
	assert(a == 1)

	done()
end

do case "resumed after events"
	local e = {}

	local a = 0
	spawn(function ()
		garbage.co = coroutine.running()
		local res, v1,v2,v3 = awaitall(e, table.unpack(types))
		assert(res == garbage)
		assert(v1 == 1)
		assert(v2 == 2)
		assert(v3 == 3)
		a = 1
		coroutine.yield()
		a = 2
	end)

	for _, e in ipairs(types) do
		assert(a == 0)
		assert(pending(e) == true)
		assert(a == 0)
		assert(emitall(e) == true)
	end
	assert(a == 0)

	coroutine.resume(garbage.co, garbage,1,2,3)
	assert(a == 1)

	assert(emitall(e) == false)
	assert(a == 1)

	done()
end

do case "ignore duplications"
	local a
	spawn(function ()
		awaitall(1,2,3,2,3)
		a = 1
	end)
	assert(a == nil)
	assert(emitall(1) == true)
	assert(emitall(2) == true)
	assert(emitall(3) == true)
	assert(a == 1)
	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)

	done()
end

do case "ignore 'nil's"
	local a
	spawn(function ()
		awaitall(nil)
		a = 1
		awaitall(1,2,nil,3)
		a = 2
		awaitall(1,2,3,nil)
		a = 3
		awaitall(nil,1,2,3)
		a = 4
		awaitall(nil,1,nil,2,3,nil)
		a = 5
	end)
	assert(a == 1)
	for i = 2, 5 do
		assert(emitall(1) == true)
		assert(emitall(2) == true)
		assert(emitall(3) == true)
		assert(a == i)
	end

	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)
	assert(emitall(nil) == false)

	done()
end

do case "different sets"
	local e1,e2,e3 = {},{},{}

	local a
	spawn(function ()
		a = 1
		awaitall(e1,e2,e3)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		a = 2
		awaitall(e1)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		a = 3
		coroutine.yield()
		a = 4
	end)
	assert(a == 1)

	local b
	spawn(function ()
		awaitall() -- no effect
		b = 1
		awaitall(e2)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		b = 2
		awaitall(e1,e3)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		b = 3
		coroutine.yield()
		b = 4
	end)
	assert(b == 1)

	assert(emitall(e1) == true)
	assert(a == 1)
	assert(b == 1)
	assert(emitall(e1) == false)
	assert(a == 1)
	assert(b == 1)

	assert(emitall(e2) == true)
	assert(a == 1)
	assert(b == 2)
	assert(emitall(e2) == false)
	assert(a == 1)
	assert(b == 2)

	assert(emitall(e3) == true)
	assert(a == 2)
	assert(b == 2)
	assert(emitall(e3) == false)
	assert(a == 2)
	assert(b == 2)

	assert(emitall(e1) == true)
	assert(a == 3)
	assert(b == 3)
	assert(emitall(e1) == false)
	assert(a == 3)
	assert(b == 3)
	assert(emitall(e2) == false)
	assert(a == 3)
	assert(b == 3)
	assert(emitall(e3) == false)
	assert(a == 3)
	assert(b == 3)

	done()
end

do case "resume order"
	local es = { {},{},{} }
	local order = counter()
	local max = 16

	local list = {}
	for i = 1, max do
		spawn(function ()
			awaitall(table.unpack(es, 1, 1+(i-1)%3))
			list[i] = order()
		end)
		assert(list[i] == nil)
	end

	assert(emitall(es[3]) == true)
	assert(#list == 0)
	assert(emitall(es[2]) == true)
	assert(#list == 0)
	assert(emitall(es[1]) == true)
	assert(#list == max)
	for i, order in ipairs(list) do
		assert(i == order)
	end

	done()
end

do case "nested emission"
	local e1,e2,e3 = {},{},{}
	local order = counter()
	local function step(list) list[#list+1] = order() end
	local a = {}
	local b = {}

	spawn(function ()
		awaitall(e1,e2,e3)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		step(a) -- a[1] = 1
		awaitall(e1,e2,e3)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == true) -- emits to other (awaiting: e2')
		assert(#b == 1 and b[1] == 2)
		step(a) -- a[2] = 3
		awaitall(e1,e2,e3)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		step(a) -- a[3] = 5
		coroutine.yield()
		step(a)
	end)

	spawn(function ()
		awaitall(e1)
		assert(emitall(e1) == false)
		assert(emitall(e2) == true) -- emits to other (awaiting: e3)
		assert(#a == 0)
		assert(emitall(e3) == true) -- resumes other (awaiting: e1,e2,e3)
		assert(#a == 1 and a[1] == 1)
		assert(emitall(e1) == true) -- emits to other (awaiting: e2,e3)
		assert(#a == 1 and a[1] == 1)
		assert(emitall(e3) == true) -- emits to other (awaiting: e2)
		assert(#a == 1 and a[1] == 1)
		step(b) -- b[1] = 2
		awaitall(e2,e3)
		assert(emitall(e1) == true) -- emits to other (awaiting: e2,e3)
		assert(#a == 2 and a[2] == 3)
		assert(emitall(e2) == true) -- emits to other (awaiting: e3)
		assert(#a == 2 and a[2] == 3)
		step(b) -- b[2] = 4
		awaitall(e3)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
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

	assert(emitall(e1) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)
	assert(emitall(e2) == false)
	assert(#a == 3 and a[3] == 5)
	assert(#b == 3 and b[3] == 6)
	assert(emitall(e3) == false)
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
	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)

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
		local a = 0
		spawn(function ()
			assert(awaitany(table.unpack(types)) == e)
			a = 1
		end)
		assert(a == 0)

		for _, e in ipairs(types) do
			assert(pending(e) == true)
			assert(a == 0)
		end

		assert(emitall(e) == true)
		assert(a == 1)

		for _, e in ipairs(types) do
			assert(emitall(e) == false)
		end

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
			assert(emitall(e) == false)
		end
		assert(a == 1)
	end

	done()
end

do case "ignore duplications"
	local a
	spawn(function ()
		awaitany(1,2,3,1,2)
		a = 1
	end)
	assert(a == nil)
	assert(emitall(2) == true)
	assert(a == 1)
	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)

	done()
end

do case "ignore 'nil's"
	local a
	spawn(function ()
		awaitany(1,2,nil,3)
		a = 1
		awaitany(1,2,3,nil)
		a = 2
		awaitany(nil,1,2,3)
		a = 3
		awaitany(nil,1,nil,2,3,nil)
		a = 4
	end)
	for i = 1, 4 do
		assert(emitall(2) == true)
		assert(a == i)
	end

	assert(emitall(1) == false)
	assert(emitall(2) == false)
	assert(emitall(3) == false)
	assert(emitall(nil) == false)

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
		assert(emitall(e1) == false) -- other is awaiting: e1'|e2|e3
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
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		step(a) -- a[2] = 6

		coroutine.yield()
		step(a)
	end)

	spawn(function ()
		local e = awaitany(e1,e2,e3)
		assert(e == e2) -- emitted from the other coroutine
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		step(b) -- b[1] = 1
		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- emitted from the other coroutine
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
		step(b) -- b[2] = 2

		local e = awaitany(e1,e2,e3)
		assert(e == e1) -- this time this is resumed first
		assert(emitall(e1) == false) -- other is awaiting: e1'|e2|e3
		step(b) -- b[3] = 4
		local e = awaitany(e1,e2,e3)
		assert(e == e1)
		assert(emitall(e1) == false)
		assert(emitall(e2) == false)
		assert(emitall(e3) == false)
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

	assert(emitall(e1) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)
	assert(emitall(e2) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)
	assert(emitall(e3) == false)
	assert(#a == 2 and a[2] == 6)
	assert(#b == 4 and b[4] == 5)

	done()
end
