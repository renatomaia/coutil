dofile "configs.lua"

local memory<const> = require "memory"
local system<const> = require "coutil.system"

local address<const> = system.address("ipv4", host, port)

assert(coroutine.resume(coroutine.create(function ()
	local passive<close> = assert(system.socket("passive", address.type))
	assert(passive:bind(address))
	assert(passive:listen(conncount))
	local stream<close> = assert(passive:accept())
	local buffer<const> = memory.create(2*msgsize)
	local missing = msgtotal*msgsize
	repeat
		missing = missing-assert(stream:read(buffer))
	until missing <= 0
end)))

assert(coroutine.resume(coroutine.create(function ()
	local stream<close> = assert(system.socket("stream", address.type))
	assert(stream:connect(address))
	for msg = 1, msgtotal do
		assert(stream:write(msgdata))
	end
end)))

system.run()
