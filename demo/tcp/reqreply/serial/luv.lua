dofile "configs.lua"

local uv = require "luv"

local expected = 0
local passive = assert(uv.new_tcp())
assert(passive:bind(host, port))
passive:listen(conncount, function (err)
	assert(not err, err)
	local stream = uv.new_tcp()
	assert(passive:accept(stream))
	expected = expected+1
	if expected == conncount then
		passive:close()
	end

	local onwrite -- forward declaration

	local bytes = 0
	local function onread(err, chunk)
		assert(not err, err)
		bytes = bytes+#chunk
		while bytes == msgsize do
			bytes = 0
			assert(stream:read_stop())
			assert(stream:write(replydata, onwrite))
		end
	end

	local replies = 0
	function onwrite(err)
		assert(not err, err)
		replies = replies+1
		if replies == msgcount then
			stream:close()
		else
			assert(stream:read_start(onread))
		end
	end

	assert(stream:read_start(onread))
end)

for i = 1, conncount do
	local stream = assert(uv.new_tcp())

	local onwrite -- forward declaration

	local replies = 0
	local bytes = 0
	local function onread(err, chunk)
		assert(not err, err)
		bytes = bytes+#chunk
		if bytes == replysize then
			bytes = 0
			replies = replies+1
			if replies == msgcount then
				stream:close()
			else
				assert(stream:read_stop())
				stream:write(msgdata, onwrite)
			end
		end
	end

	function onwrite(err)
		assert(not err, err)
		assert(stream:read_start(onread))
	end

	stream:connect(host, port, function (err)
		assert(not err, err)
		assert(stream:write(msgdata, onwrite))
	end)
end

uv.run()
