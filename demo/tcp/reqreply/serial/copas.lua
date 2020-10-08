dofile "configs.lua"

local socket = require "socket"
local copas = require "copas"

local handlers = 0
local passive = socket.bind(host, port)
copas.addserver(passive, function (stream)
	handlers = handlers+1
	if handlers == conncount then
		copas.removeserver(passive)
	end
	for msg = 1, msgcount do
		assert(copas.receive(stream, msgsize))
		assert(copas.send(stream, replydata))
	end
	assert(stream:close())
end)

for i = 1, conncount do
	copas.addthread(function ()
		local stream = socket.tcp()
		assert(copas.connect(stream, host, port))
		for msg = 1, msgcount do
			assert(copas.send(stream, msgdata))
			assert(copas.receive(stream, replysize))
		end
		assert(stream:close())
	end)
end

copas.loop()
