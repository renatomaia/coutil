dofile "configs.lua"

local memory<const> = require "memory"
local system<const> = require "coutil.system"

local address<const> = system.address("ipv4", host, port)
local buffsize<const> = 8129

local function doconn(stream)
	local pending = 0

	local replier<const> = coroutine.create(function ()
		while true do
			repeat
				assert(stream:write(replydata))
				pending = pending-1
			until pending == 0
			coroutine.yield()
		end
	end)

	local requests = 0
	local bytes = 0
	local buffer<const> = memory.create(buffsize)
	repeat
		repeat
			bytes = bytes+assert(stream:read(buffer, bytes+1))
		until bytes >= msgsize
		repeat
			bytes = bytes-msgsize
			requests = requests+1
			pending = pending+1
			if pending == 1 then
				assert(coroutine.resume(replier))
			end
		until bytes < msgsize
	until requests == msgcount
end

assert(coroutine.resume(coroutine.create(function ()
	local passive<close> = system.socket("passive", address.type)
	assert(passive:bind(address))
	assert(passive:listen(conncount))
	for i = 1, conncount do
		assert(coroutine.resume(coroutine.create(doconn), assert(passive:accept())))
	end
end)))

for i = 1, conncount do
	assert(coroutine.resume(coroutine.create(function ()
		local stream<close> = system.socket("stream", address.type)
		assert(stream:connect(address))

		assert(coroutine.resume(coroutine.create(function ()
			for msg = 1, msgcount do
				assert(stream:write(msgdata))
			end
		end)))

		local buffer<const> = memory.create(buffsize)
		local bytes = msgcount*replysize
		repeat
			bytes = bytes-assert(stream:read(buffer))
		until bytes == 0
	end)))
end

system.run()
