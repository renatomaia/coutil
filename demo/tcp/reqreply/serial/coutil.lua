dofile "configs.lua"

local memory = require "memory"
local system = require "coutil.system"

local address<const> = system.address("ipv4", host, port)

local function doconn(...)
	local stream<close> = ...
	local buffer<const> = memory.create(msgsize)
	for msg = 1, msgcount do
		local bytes = 0
		repeat
			bytes = bytes+assert(stream:read(buffer, bytes+1))
		until bytes == msgsize
		assert(stream:write(replydata))
	end
end

assert(coroutine.resume(coroutine.create(function ()
	local passive<close> = assert(system.socket("passive", address.type))
	assert(passive:bind(address))
	assert(passive:listen(conncount))
	for i = 1, conncount do
		assert(coroutine.resume(coroutine.create(doconn), assert(passive:accept())))
	end
end)))

for i = 1, conncount do
	assert(coroutine.resume(coroutine.create(function ()
		local stream<close> = assert(system.socket("stream", address.type))
		assert(stream:connect(address))
		local buffer = memory.create(replysize)
		for msg = 1, msgcount do
			assert(stream:write(msgdata))
			local bytes = 0
			repeat
				bytes = bytes+assert(stream:read(buffer, bytes+1))
			until bytes == replysize
		end
	end)))
end

system.run()
