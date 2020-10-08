dofile "configs.lua"

local socket = require "socket"
local copas = require "copas"

copas.autoclose = false

local handlers = 0
local passive = socket.bind(host, port)
copas.addserver(passive, function (stream)
	handlers = handlers+1
	if handlers == conncount then
		copas.removeserver(passive)
	end

	local pending = 0

	local replier = copas.addthread(function ()
		while true do
			copas.sleep(-1)
			repeat
				assert(copas.send(stream, replydata))
				pending = pending-1
			until pending == 0
		end
	end)

	for msg = 1, msgcount do
		assert(copas.receive(stream, msgsize))
		pending = pending+1
		if pending == 1 then
			copas.wakeup(replier)
		end
	end
end)

for i = 1, conncount do
	copas.addthread(function ()
		local stream = socket.tcp()
		assert(copas.connect(stream, host, port))

		copas.addthread(function ()
			for msg = 1, msgcount do
				assert(copas.receive(stream, replysize))
			end
			assert(stream:close())
		end)

		for msg = 1, msgcount do
			assert(copas.send(stream, msgdata))
		end
	end)
end

copas.loop()