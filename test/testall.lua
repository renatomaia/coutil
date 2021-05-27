local system = require "coutil.system"
dofile "utils.lua"

local test

function newgroup(title)
	print("\n--- "..title.." "..string.rep("-", 65-#title))
end

function newtest(title)
	test = title
	print()
end

function case(title)
	io.write("[",test,"]",string.rep(" ", 10-#test),title," ...")
	io.flush()
end

function done()
	assert(spawnerr == nil)
	collectgarbage("collect")
	assert(next(garbage) == nil)
	print(" OK")
	assert(system.run() == false)
end

dofile "info.lua"
dofile "event.lua"
dofile "queued.lua"
dofile "promise.lua"
dofile "mutex.lua"
dofile "spawn.lua"
dofile "system.lua"
dofile "time.lua"
dofile "file.lua"
if standard == "posix" then dofile "signal.lua" end
dofile "netaddr.lua"
dofile "stream.lua"
dofile "network.lua"
dofile "pipe.lua"
dofile "process.lua"
dofile "coroutine.lua"
dofile "thread.lua"
dofile "stdio.lua"
if standard == "posix" then dofile "operation.lua" end

print "\nSuccess!\n"
