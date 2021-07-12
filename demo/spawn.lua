local spawn = require "coutil.spawn"

local function yieldargs(...)
	local name = tostring(coroutine.running())
	print(string.format("%s: started", name))
	for i = 1, select("#", ...) do
		coroutine.yield()
		local value = select(i, ...)
		print(string.format("%s: step %d", name, value))
	end
	print(string.format("%s: ended", name))
	return ...
end

local queue = {}

-- use 'spawn.catch' to run threads that reports any uncatched errors.

local function onerror(errmsg)
	local name = tostring(coroutine.running())
	local trace = debug.traceback(errmsg)
	io.stderr:write(name, ": ", trace, "\n")
end

table.insert(queue, spawn.catch(onerror, yieldargs, 1,2,3))
table.insert(queue, spawn.catch(onerror, yieldargs, 1,nil,3)) -- interrupted by an error

-- use 'spanw.trap' to run threads with a terminaning function.

local function onfinish(ok, ...)
	if ok then
		local name = tostring(coroutine.running())
		print(name..": returned", ...)
	else
		onerror(...)
	end
end

table.insert(queue, spawn.trap(onfinish, yieldargs, 1,2,3))
table.insert(queue, spawn.trap(onfinish, yieldargs, 1,nil,3)) -- interrupted by an error

-- resume all threads in 'queue' until they all are dead.

while #queue > 0 do
	print("--- resuming "..#queue.." suspended threads -------------------------")
	local i = 1
	while i <= #queue do
		local thread = queue[i]
		coroutine.resume(thread)  -- ignores any results
		if coroutine.status(thread) == "suspended" then
			i = i+1
		else
			table.remove(queue, i)
		end
	end
end
