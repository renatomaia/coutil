-- Adaptation of demo from libuv repository:
-- https://github.com/libuv/libuv/blob/v1.x/docs/code/multi-echo-server/main.c

local spawn = require "coutil.spawn"
local system = require "coutil.system"

spawn.catch(print, function ()
	-- setup workers
	local workers = {}
	for i = 1, #system.cpuinfo() do
		workers[i] = {
			execfile = arg[-1],
			arguments = { "worker.lua" },
			stdin = "ws",
			stdout = false,
		}
		spawn.catch(print, system.execute, workers[i])
	end

	-- wait for clients
	local addr = system.address("ipv4", "0.0.0.0", 7000)
	local passive = system.socket("passive", addr.type)
	assert(passive:bind(addr))
	assert(passive:listen(128))
	for i = 0, math.huge do
		local stream = assert(passive:accept())
		local worker = workers[1 + i % #workers]
		assert(worker.stdin:write("a", 1, 1, stream))
	end
end)

system.run()
