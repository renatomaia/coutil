local stateco = require "coutil.coroutine"
local system = require "coutil.system"

-- body of a state coroutine that executes function 'os.execute' each time
-- it is resumed.

local os_execute = [[
	local coroutine = require "coroutine"
	local os = require "os"
	local command = ...
	while true do
		os.execute(command)
		command = coroutine.yield()
	end
]]

-- execute three threads that run 'os.execute' as an await function using a
-- state coroutine.

coroutine.resume(coroutine.create(function ()
	local co<close> = assert(stateco.load(os_execute))
	for i = 1, 12 do
		system.resume(co, "sleep .1")  -- suspend awaiting 'os.execute'.
		io.write("A")                  -- show its progress.
		io.flush()
	end
end))

coroutine.resume(coroutine.create(function ()
	local co<close> = assert(stateco.load(os_execute))
	for i = 1, 6 do
		system.resume(co, "sleep .2")  -- suspend awaiting 'os.execute'.
		io.write("B")                  -- show its progress.
		io.flush()
	end
end))

coroutine.resume(coroutine.create(function ()
	local co<close> = assert(stateco.load(os_execute))
	for i = 1, 3 do
		system.resume(co, "sleep .4")  -- suspend awaiting 'os.execute'.
		io.write("C")                  -- show its progress.
		io.flush()
	end
end))

system.run()  -- resumes suspended coroutines awaiting conditions.

print()
