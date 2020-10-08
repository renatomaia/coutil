dofile "configs.lua"

local socket = require "socket"
local copas = require "copas"

local passive = assert(socket.bind(host, port))
copas.addserver(passive, function (stream)
	copas.removeserver(passive)
	for msg = 1, msgtotal do
		assert(copas.receive(stream, msgsize))
	end
	assert(stream:close())
end)

copas.addthread(function ()
	local stream = socket.tcp()
	assert(copas.connect(stream, host, port))
	for msg = 1, msgtotal do
		assert(copas.send(stream, msgdata))
	end
	assert(stream:close())
end)

copas.loop()
