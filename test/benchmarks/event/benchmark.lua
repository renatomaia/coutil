local BasicSetup = [[
	local suspend, resume = %s
	local thread = false
	local count = 0
	for i = 1, string.find(_CASE_, "[QE]1$") and 1e3 or 1 do
		local coro = coroutine.create(function ()
			while true do
				suspend(thread)
				count = count+1
			end
		end)
		thread = thread or coro
		coroutine.resume(coro)
	end
]]
local EventSetup = [[
	local event = require "coutil.event"
]]..BasicSetup
local QueueSetup = [[
	local event = require "coutil.queued"
]]..BasicSetup

local GetTime = [[(function ()
	local now = require("coutil.system").nanosecs
	return function () return now()*1e-9 end
end)()]]

return {
	repeats = 3e5,
	gettime = GetTime,
	test = "resume(thread);assert(count == _ITERATION_)",
	cases = {
		Yield     = { setup = BasicSetup:format[[coroutine.yield, coroutine.resume]] },
		["A*:E*"] = { setup = EventSetup:format[[event.awaitany , event.emitall]] },
		["A*:E1"] = { setup = EventSetup:format[[event.awaitany , event.emitone]] },
		["A1:E*"] = { setup = EventSetup:format[[event.await    , event.emitall]] },
		["A1:E1"] = { setup = EventSetup:format[[event.await    , event.emitone]] },
		["Q*:Q*"] = { setup = QueueSetup:format[[event.awaitany , event.emitall]] },
		["Q*:Q1"] = { setup = QueueSetup:format[[event.awaitany , event.emitone]] },
		["Q1:Q*"] = { setup = QueueSetup:format[[event.await    , event.emitall]] },
		["Q1:Q1"] = { setup = QueueSetup:format[[event.await    , event.emitone]] },
	}
}
