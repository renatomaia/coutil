-- Adaptation of demo from libuv repository:
-- https://github.com/libuv/libuv/blob/v1.x/docs/code/multi-echo-server/worker.c

local memory = require "memory"
local system = require "coutil.system"
local spawn = require "coutil.spawn"

local function report(errmsg)
	io.stderr:write(debug.traceback(errmsg), "\n")
end

local function echo_read(...)
	local stream<close> = ...
	local buffer = memory.create(8192)
	while true do
		local bytes, errmsg = stream:read(buffer)
		if not bytes then
			if errmsg == "end of file" then
				return
			end
			error(errmsg)
		end
		assert(stream:write(buffer, 1, bytes))
	end
end

spawn.catch(report, function ()
	local pid = system.procinfo("#")
	local buffer = memory.create(1)
	while true do
		local bytes, stream = assert(system.stdin:read(buffer))
		system.stderr:write("Worker "..pid..": accepted stream\n")
		spawn.catch(report, echo_read, stream)
	end
end)

system.run()
