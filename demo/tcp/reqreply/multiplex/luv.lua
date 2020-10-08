dofile "configs.lua"

local uv = require "luv"

local expected = 0
local passive = assert(uv.new_tcp())
assert(passive:bind(host, port))
assert(passive:listen(conncount, function (err)
	assert(not err, err)
	local stream = uv.new_tcp()
	assert(passive:accept(stream))
	expected = expected+1
	if expected == conncount then
		passive:close()
	end

	local replies = 0
	local function onwrite(err)
		assert(not err, err)
		replies = replies+1
		if replies == msgcount then
			stream:close()
		end
	end

	local bytes = 0
	stream:read_start(function (err, chunk)
		assert(not err, err)
		bytes = bytes+#chunk
		while bytes >= msgsize do
			bytes = bytes-msgsize
			stream:write(replydata, onwrite)
		end
	end)
end))

for i = 1, conncount do
	local stream = assert(uv.new_tcp())

	local sent = 0
	local function onwrite(err)
		assert(not err, err)
		sent = sent+1
		if sent < msgcount then
			stream:write(msgdata, onwrite)
		end
	end

	stream:connect(host, port, function (err)
		assert(not err, err)
		stream:write(msgdata, onwrite)
		local replies = 0
		local bytes = 0
		stream:read_start(function (err, chunk)
			assert(not err, err)
			bytes = bytes+#chunk
			while bytes >= replysize do
				bytes = bytes-replysize
				replies = replies+1
				if replies == msgcount then
					stream:close()
				end
			end
		end)
	end)
end

uv.run()
