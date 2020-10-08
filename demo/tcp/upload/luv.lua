dofile "configs.lua"

local uv = require "luv"

local passive = assert(uv.new_tcp())
assert(passive:bind(host, port))
assert(passive:listen(conncount, function (err)
	assert(not err, err)
	local stream = uv.new_tcp()
	assert(passive:accept(stream))
	local missing = msgtotal*msgsize
	assert(stream:read_start(function (err, chunk)
		missing = missing-#chunk
		if missing <= 0 then
			stream:close()
			passive:close()
		end
	end))
end))

local stream = uv.new_tcp()
local missing = msgtotal
local function onwrite(err)
	assert(not err, err)
	missing = missing-1
	if missing > 0 then
		assert(stream:write(msgdata, onwrite))
	else
		stream:close()
	end
end
stream:connect(host, port, function (err)
	assert(not err, err)
	assert(stream:write(msgdata, onwrite))
end)

uv.run()
