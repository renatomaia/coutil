local system = require "coutil.system"
local channel = require "coutil.channel"
local preemptco = require "coutil.coroutine"

local function title(name, suffix)
	if suffix ~= nil then
		name = name.." "..suffix
	end
	case(name)
end

local oncallbacks do
	local ch = channel.create("channel used to run in UV pool I/O phase")
	function oncallbacks(f, ...)
		spawn(function (...)
			system.awaitch(ch)
			return f(...)
		end, ...)
		channel.sync(ch)
	end
end

local pco = preemptco.load([===[
local coroutine = require "coroutine"
repeat until coroutine.yield()
]===])

local function testReqOp(op, suffix) -- O1,C1,Y1
end

local function testReqOp(op, suffix) -- O1,C1,(O3,C1,)+Y1
end

local function testReqOp(op, suffix) -- O1,C1,O4,C5,Y2,F1
end

local function testReqOp(op, suffix) -- O1,R1,C2
end

local function testReqOp(op, suffix) -- O1,R1,(O5,R2,)+C2
end

local function testReqOp(op, suffix) -- O1,R1,O5,C3,C1,Y1
end

local function testReqOp(op, suffix) -- O1,R1,O5,C4,C5,Y2,F1
end

-- Thread Operation Tests

local function testThrOpAndYield(op, suffix) -- O2,C5,Y2,F1
	title("scheduled yield", suffix)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O2
		a = true
		coroutine.yield() -- Y2
		a = false
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testThrOpTwice(op, suffix) -- O2,C5,(O6,C5,)+Y2,F1
	title("reschedule", suffix)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O2
		a = 1
		op:await(2) -- O6
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		op:trigger(2) -- ...C5
	end)

	gc()
	assert(system.run("step") == true) -- C5,06
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testThrOpBeforeCanceledOp(op, suffix) -- O2,C5,Y2,(O10,R6,)+F1
	title("yield and cancel repeat", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		op:await(1, "fail") -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
		coroutine.resume(garbage.co, nil, "canceled") -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run("step") == false) -- C5,Y2,O10,R6,F1
	assert(a == true)

	done()
end

local function testThrOpBeforeReqOp(op, suffix) -- O2,C5,Y2,O10,F2,C1,Y1
	title("yield and call request op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		system.resume(pco) -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run("step") == true) -- C5,Y2,O10,F2
	assert(a == 2)

	gc()
	assert(system.run("step") == false) -- C1,Y1
	assert(a == true)

	done()
end

local function testThrOpBeforeOtherThrOp(op, suffix) -- O2,C5,Y2,O10,F3,C5,Y2,F1
	title("yield and call thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		coroutine.yield() -- Y2
		a = 2
		system.suspend() -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run("step") == true) -- C5,Y2,O10,F3
	assert(a == 2)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testThrOpAndCanceledOp(op, suffix) -- O2,C5,O7,R6,F1
	title("and cancel thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		local ok, res = system.suspend() -- O7
		assert(not ok)
		assert(res == "canceled")
		a = true
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run("step") == false) -- C5,O7,R6,F1
	assert(a == true)

	done()
end

local function testThrOpAndCanceledOpTwice(op, suffix) -- O2,C5,O7,R6,(O10,R6,)+F1
	title("and cancel op. twice", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		local ok, res = system.suspend() -- O7
		assert(not ok)
		assert(res == "canceled")
		a = 2
		local ok, res = system.suspend() -- O10
		assert(not ok)
		assert(res == "canceled")
		a = true
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	oncallbacks(function ()
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == 2)
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run("step") == false) -- C5,O7,R6,(O10,R6,)+F1
	assert(a == true)

	done()
end

local function testThrOpAndReqOp(op, suffix) -- O2,C5,O7,F2,C1,Y1
	title("and request op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		assert(system.resume(pco)) -- O7
		a = true
	end) -- Y1
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	gc()
	assert(system.run("step") == true) -- C5,O7,F2
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C1,Y1
	assert(a == true)

	done()
end

local function testThrOpAndOtherThrOp(op, suffix) -- O2,C5,O7,F3,C5,Y2,F1
	title("and thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O2
		a = 1
		assert(system.suspend()) -- O7
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C5

	gc()
	assert(system.run("step") == true) -- C5,O7,F3
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testThrOpResumed(op, suffix) -- O2,R4,F1
	title("cancel schedule", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = true
		coroutine.yield()
		a = false
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R4
	assert(a == true)

	gc()
	assert(system.run("step") == false) -- F1
	assert(a == true)

	done()
end

local function testThrOpResumedAndCanceledOp(op, suffix) -- O2,R4,(O10,R6,)+F1
	title("cancel schedule twice", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = 1
		op:await(2, "fail") -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R4,O10
	assert(a == 1)

	oncallbacks(function () -- (C5,O6)?
		coroutine.resume(garbage.co, nil, "canceled") -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run("step") == false) -- F1
	assert(a == true)

	done()
end

local function testThrOpResumedAndReqOp(op, suffix) -- O2,R4,O10,F2,C1,Y1
	title("cancel schedule and request op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = 1
		system.resume(pco) -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R4
	assert(a == 1)

	gc()
	assert(system.run("step") == true) -- F2
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C1,Y1
	assert(a == true)

	done()
end

local function testThrOpResumedAndOtherThrOp(op, suffix) -- O2,R4,O10,F3,C5,Y2,F1
	title("cancel schedule and thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = 1
		system.suspend() -- O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R4
	assert(a == 1)

	gc()
	assert(system.run("step") == true) -- F3
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testThrOpResumed2(op, suffix) -- O2,(R3,C6|R4),F1
	title("cancel schedule", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = true
		coroutine.yield()
		a = false
	end)
	assert(a == 0)

	oncallbacks(function ()
		op:trigger(1) -- ...C6?

		gc()
		coroutine.resume(garbage.co, nil, "canceled") -- R3|R4
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- (R3,C6|R4),F1
	assert(a == true)

	done()
end

local function testThrOpResumedAndCanceledOp2(op, suffix) -- O2,(R3,C6|R4),(O10,R6,)+F1
	title("cancel schedule twice", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O2
		a = 1
		op:await(2, "fail") -- O10
		a = true
	end)
	assert(a == 0)

	oncallbacks(function ()
		op:trigger(1) -- ...C6?

		gc()
		coroutine.resume(garbage.co, nil, "canceled") -- (R3,C6|R4),O10
		assert(a == 1)
		coroutine.resume(garbage.co, nil, "canceled") -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- (R3,C6|R4),(O10,R6,)+F1
	assert(a == true)

	done()
end

local function testThrOpResumedAndReqOp2(op, suffix) -- O2,(R3,C6|R4),O10,F2,C1,Y1
end

local function testThrOpResumedAndThrOp2(op, suffix) -- O2,(R3,C6|R4),O10,F3,C5,Y2,F1
end

local function testThrOpNoCancelReschedule(op, suffix) -- O2,R3,O8,C5,Y2,F1
end

local function testThrOpNoCancelReset(op, suffix) -- O2,R3,C6,F1
end

-- Cases ignored:
-- O2,R3,O8,(C5,O6,)+C5,Y2,F1
-- O2,R3,O8,C5,Y2,(O10,R6,)+F1
-- O2,R3,O8,C5,Y2,O10,F2,C1,Y1
-- O2,R3,O8,C5,Y2,O10,F3,C5,Y2,F1
-- O2,R3,O8,C5,O7,R6,F1
-- O2,R3,O8,C5,O7,R6,(O10,R6,)+F1
-- O2,R3,O8,C5,O7,F2,C1,Y1
-- O2,R3,O8,C5,O7,F3,C5,Y2,F1
-- O2,R3,O8,R4,F1
-- O2,R3,O8,R4,(O10,R6,)+F1
-- O2,R3,O8,R4,O10,F2,C1,Y1
-- O2,R3,O8,R4,O10,F3,C5,Y2,F1
-- O2,R3,(O9,R5,)+C6,F1
-- O2,R3,O9,C7,R6,F1
-- O2,R3,O9,C7,R6,(O10,R6,)+F1
-- O2,R3,O9,C7,F2,C1,Y1
-- O2,R3,O9,C7,F3,C5,Y2,F1

local function testThrOp(op)
	testThrOpAndYield(op)
	testThrOpTwice(op)
	testThrOpBeforeCanceledOp(op)
	testThrOpBeforeReqOp(op)
	testThrOpBeforeOtherThrOp(op)
	testThrOpAndCanceledOp(op)
	testThrOpAndCanceledOpTwice(op)
	testThrOpAndReqOp(op)
	testThrOpAndOtherThrOp(op)
	testThrOpResumed(op)
	testThrOpResumedAndCanceledOp(op)
	testThrOpResumedAndReqOp(op)
	testThrOpResumedAndOtherThrOp(op)
	testThrOpNoCancelReschedule(op)
	testThrOpNoCancelReset(op)
end

do newtest "channels" ----------------------------------------------------------

	local table = require "loop.table"
	local oo = require "loop.base"


	local op = oo.class{
		channels = { channel.create("A"), channel.create("B") }
	}

	function op:await(cfgid, fail)
		local ok, res = system.awaitch(self.channels[cfgid], "in")
		if not fail then
			assert(ok)
			assert(res == "Secret Token "..cfgid)
		else
			assert(not ok)
			assert(res == "canceled")
		end
	end

	function op:trigger(cfgid)
		assert(channel.sync(self.channels[cfgid], "out", "Secret Token "..cfgid))
	end

	local cancancel = op{ trigger = function () end }
	testThrOpResumed(cancancel)
	testThrOpResumedAndCanceledOp(cancancel)
	testThrOpResumedAndReqOp(cancancel)
	testThrOpResumedAndOtherThrOp(cancancel)

	testThrOp(op())

	local defaultch = channel.create("Default Test Channel")
	local samech = op{ channels = { defaultch, defaultch } }
	testThrOpTwice(samech, "different channel")
end
