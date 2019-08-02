dofile "utils.lua"

local memory = require "memory"
local system = require "coutil.system"

local argc = select("#", ...)-2
local format = szfmt..string.rep("z", argc)
local buffer = memory.create(szlen+maxlen)
local _, index = assert(memory.pack(buffer, format, szlen+1, argc-1, select(3, ...)))
assert(memory.pack(buffer, szfmt, 1, index-szlen-1))

spawn(function ()
	local address = assert(system.findaddr(host, port, "s6"))()
	local conn = assert(system.socket("stream", address.type))
	assert(conn:connect(address))
	assert(conn:send(buffer, 1, index-1))
	local bytes = 0
	while bytes < szlen do
		bytes = bytes+assert(conn:receive(buffer, bytes+1))
	end
	local size, index = memory.unpack(buffer, szfmt)
	assert(size <= maxlen, "out of memory")
	while bytes < szlen+size do
		bytes = bytes+assert(conn:receive(buffer, bytes+1))
	end
	conn:close()
	print(">", memory.tostring(buffer, szlen+1, bytes))
end)

system.run()
