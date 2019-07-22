dofile "utils.lua"

local luv = require "luv"

local argc = select("#", ...)-2
local format = szfmt..string.rep("z", argc)
local message = {nil, string.pack(format, argc-1, select(3, ...))}
message[1] = string.pack(szfmt, #message[2])

local function readsize(chunk)
	local size, index = string.unpack(szfmt, chunk)
	chunk = string.sub(chunk, index)
	return print, { chunk }, size-#chunk
end

local flags = {
	socktype = "stream",
	family = "inet6",
}
assert(luv.getaddrinfo(host, port, flags, function (err, addresses)
	assert(not err, err)
	local address = assert(addresses[1])
	local conn = assert(luv.new_tcp())
	assert(conn:connect(address.addr, address.port, function (err)
		assert(not err, err)
		assert(conn:write(message, function (err)
			assert(not err, err)
			local buffer = {}
			local missing = szlen
			local action = readsize
			assert(conn:read_start(function (err, chunk)
				assert(not err, err)
				table.insert(buffer, chunk)
				missing = missing-#chunk
				while missing <= 0 do
					chunk = table.concat(buffer)
					action, buffer, missing = action(chunk)
					if action == nil then
						conn:read_stop()
						conn:close()
						break
					end
				end
			end))
		end))
	end))
end))

luv.run()
