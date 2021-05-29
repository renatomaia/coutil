dofile "utils.lua"

local memory<const> = require "memory"
local system<const> = require "coutil.system"

local argc<const> = select("#", ...)-2
local format<const> = szfmt..string.rep("z", argc)
local buffer<const> = memory.create(szlen+maxlen)
local _, index = assert(memory.pack(buffer, format, szlen+1, argc-1, select(3, ...)))
assert(memory.pack(buffer, szfmt, 1, index-szlen-1))

spawn(function ()
	local address = assert(system.findaddr(host, port, "s6")):getaddress()
	local conn<close> = assert(system.socket("stream", address.type))
	assert(conn:connect(address))
	assert(conn:write(buffer, 1, index-1))
	local bytes = 0
	while bytes < szlen do
		bytes = bytes+assert(conn:read(buffer, bytes+1))
	end
	local size, index = memory.unpack(buffer, szfmt)
	assert(size <= maxlen, "out of memory")
	while bytes < szlen+size do
		bytes = bytes+assert(conn:read(buffer, bytes+1))
	end
	print(">", memory.tostring(buffer, szlen+1, bytes))
end)

system.run()
