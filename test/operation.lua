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

-- Thread Operation Tests

local function testOpAndYield(op, suffix) -- O2,C5,Y2,F1
                                          -- O1,C1,Y1
	title("scheduled yield", suffix)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O1|O2
		a = true
		coroutine.yield() -- Y1|Y2
		a = false
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	gc()
	assert(system.run("step") == false) -- (C1,Y1|C5,Y2,F1)
	assert(a == true)

	done()
end

local function testOpTwice(op, suffix) -- O2,C5,(O6,C5,)+Y2,F1
                                       -- O1,C1,(O3,C1,)+Y1
	title("reschedule", suffix)

	local a
	spawn(function ()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		op:await(2) -- O3|O6
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		op:trigger(2) -- ...C1|C5
	end)

	gc()
	assert(system.run() == false) -- (C1,O3,C1,Y1|C5,06,C5,Y2,F1)
	assert(a == true)

	done()
end

local function testOpBeforeCancelOp(op, suffix) -- O2,C5,Y2,(O10,R6,)+F1
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
		while a < 1 do system.suspend() end
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
		coroutine.resume(garbage.co, nil, "canceled") -- R6
		assert(a == true)
		op:trigger(1) -- ...C5
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,R6,F1
	assert(a == true)

	done()
end

local function testOpBeforeReqOp(op, suffix) -- O2,C5,Y2,O10,F2,C1,Y1
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
		while a < 1 do system.suspend() end
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,F2,C1,Y1
	assert(a == true)

	done()
end

local function testOpBeforeThrOp(op, suffix) -- O2,C5,Y2,O10,F3,C5,Y2,F1
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
		while a < 1 do system.suspend() end
		coroutine.resume(garbage.co) -- O10
		assert(a == 2)
	end)

	gc()
	assert(system.run() == false) -- C5,Y2,O10,F3,C5,Y2,F1
	assert(a == true)

	done()
end

local function testOpAndCancelOp(op, suffix) -- O2,C5,O7,R6,F1
                                             -- O1,C1,O4,R4,F1
	title("and cancel thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		local ok, res = system.suspend() -- O4|O7
		assert(not ok)
		assert(res == "canceled")
		a = true
	end)
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	oncallbacks(function ()
		while a < 1 do system.suspend() end
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R4|R6
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- (C1,O4,R4|C5,O7,R6),F1
	assert(a == true)

	done()
end

local function testOpAndCancelOpTwice(op, suffix) -- O2,C5,O7,R6,(O10,R6,)+F1
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
		while a < 1 do system.suspend() end
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == 2)
		assert(coroutine.resume(garbage.co, nil, "canceled")) -- R6
		assert(a == true)
	end)

	gc()
	assert(system.run() == false) -- C5,O7,R6,(O10,R6,)+F1
	assert(a == true)

	done()
end

local function testOpAndReqOp(op, suffix) -- O2,C5,O7,F2,C1,Y1
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

local function testOpAndThrOp(op, suffix) -- O2,C5,O7,F3,C5,Y2,F1
                                          -- O1,C1,O4,C5,Y2,F1
	title("and thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1) -- O1|O2
		a = 1
		assert(system.suspend()) -- O4|O7
		a = true
	end) -- Y2
	assert(a == 0)
	gc()

	op:trigger(1) -- ...C1|C5

	gc()
	assert(system.run("step") == true) -- (C1,O4|C5,O7,F3)
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

local function testOpResumed(op, suffix) -- O2,R4,F1
                                         -- O1,R1,C2
	title("cancel schedule", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = true
		coroutine.yield()
		a = false
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R1|R4
	assert(a == true)

	gc()
	assert(system.run("step") == false) -- (R1,C2|R4,F1)
	assert(a == true)

	done()
end

local function testOpResumedTwice(op, suffix) -- O2,R4,(O10,R6,)+F1
                                              -- O1,R1,(O5,R2,)+C2
	title("cancel schedule twice", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		op:await(2, "fail") -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- (R1,O5|R4,O10)
	assert(a == 1)

	oncallbacks(function () -- (C5,O6)?
		coroutine.resume(garbage.co, nil, "canceled") -- R2|R6
		assert(a == true)
		op:trigger(2)
	end)

	gc()
	assert(system.run() == false) -- (R1,O5,R2,C2|R4,O10,R6,F1)
	assert(a == true)

	done()
end

local function testOpResumedAndReqOp(op, suffix) -- O2,R4,O10,F2,C1,Y1
                                                 -- O1,R1,O5,C3,C1,Y1
	title("cancel schedule and request op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		system.resume(pco) -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R1|R4
	assert(a == 1)

	gc()
	assert(system.run("step") == true) -- (O1,R1,O5,C3|R4,O10,F2)
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C1,Y1
	assert(a == true)

	done()
end

local function testOpResumedAndThrOp(op, suffix) -- O2,R4,O10,F3,C5,Y2,F1
                                                 -- O1,R1,O5,C4,C5,Y2,F1
	title("cancel schedule and thread op.", suffix)

	local a
	spawn(function ()
		garbage.co = coroutine.running()
		a = 0
		op:await(1, "fail") -- O1|O2
		a = 1
		system.suspend() -- O5|O10
		a = true
	end)
	assert(a == 0)

	op:trigger(1)

	coroutine.resume(garbage.co, nil, "canceled") -- R1|R4
	assert(a == 1)

	gc()
	assert(system.run("step") == true) -- (R1,O5,C4|R4,O10,F3)
	assert(a == 1)

	gc()
	assert(system.run("step") == false) -- C5,Y2,F1
	assert(a == true)

	done()
end

-- Special Cases:
-- O2,R3,C6,F1
-- O2,R3,C6,(O10,R6,)+F1
-- O2,R3,C6,O10,F2,C1,Y1
-- O2,R3,C6,O10,F3,C5,Y2,F1
-- O2,R3,O8,C5,Y2,F1
-- O2,R3,C6,F1

-- Ignored Cases:
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

local function testOp(op)
	testOpAndYield(op)
	testOpTwice(op)
	testOpBeforeCancelOp(op)
	testOpBeforeReqOp(op)
	testOpBeforeThrOp(op)
	testOpAndCancelOp(op)
	testOpAndCancelOpTwice(op)
	testOpAndReqOp(op)
	testOpAndThrOp(op)
	testOpResumed(op)
	testOpResumedTwice(op)
	testOpResumedAndReqOp(op)
	testOpResumedAndThrOp(op)
end

local function checkAwait(op, cfgid, ...)
	local ok, err = ...
	if not cfgid then
		assert(not ok)
		assert(err == "canceled")
	else
		if op.check == nil then
			assert(ok)
		else
			op:check(cfgid, ...)
		end
	end
end

local function newTestOpCase(op)
	local await = op.await
	function op:await(cfgid, fail)
		return checkAwait(op, not fail and cfgid or nil, await(self, cfgid))
	end
	return op
end

do newtest "coroutine" --------------------------------------------------------------
	local chunk = utilschunk..[[
		local coroutine = require "coroutine"
		local cfgid = ...
		local path = "signal"..cfgid..".tmp"
		local token = "Secret Token "..cfgid
		repeat
			waitsignal(path)
		until coroutine.yield(token) ~= cfgid
	]]
	testOp(newTestOpCase{
		coroutines = { preemptco.load(chunk), preemptco.load(chunk) },
		await = function (self, cfgid)
			return system.resume(self.coroutines[cfgid], cfgid)
		end,
		trigger = function (self, cfgid)
			sendsignal("signal"..cfgid..".tmp")
		end,
		check = function (self, cfgid, ok, res)
			assert(ok == true)
			assert(res == "Secret Token "..cfgid)
		end,
	})
end

do newtest "time" --------------------------------------------------------------
	testOp(newTestOpCase{
		await = function (self, cfgid)
			return system.suspend(cfgid / 100)
		end,
		trigger = function (self, cfgid)
			local future = system.time() + cfgid / 100
			repeat until system.time("update") >= future
		end,
	})
end

do newtest "suspend" -----------------------------------------------------------
	testOp(newTestOpCase{
		await = function (self, cfgid)
			return system.suspend()
		end,
		trigger = function (self, cfgid)
			-- empty
		end,
	})
end

do newtest "signal" ------------------------------------------------------------
	testOp(newTestOpCase{
		signals = { "winresize", "child" },
		await = function (self, cfgid)
			return system.awaitsig(self.signals[cfgid])
		end,
		trigger = function (self, cfgid)
			system.emitsig(system.getpid(), self.signals[cfgid])
		end,
		check = function (self, cfgid, signal)
			assert(signal == self.signals[cfgid])
		end,
	})
end

do newtest "channels" ----------------------------------------------------------
	local op = newTestOpCase{
		channels = { channel.create("A"), channel.create("B") },
		await = function (self, cfgid)
			return system.awaitch(self.channels[cfgid], "in")
		end,
		trigger = function (self, cfgid)
			self.channels[cfgid]:sync("out", "Secret Token "..cfgid)
		end,
		check = function (self, cfgid, ok, res)
			assert(ok == true)
			assert(res == "Secret Token "..cfgid)
		end,
	}

	testOp(op)

	local defaultch = channel.create("Default Test Channel")
	op.channels = { defaultch, defaultch }
	testOpTwice(op, "with the same channel")

	op.trigger = function () end
	op.check = nil
	local title = "allowing cancellation"
	testOpResumed(op, title)
	testOpResumedTwice(op, title)
	testOpResumedAndReqOp(op, title)
	testOpResumedAndThrOp(op, title)
end
