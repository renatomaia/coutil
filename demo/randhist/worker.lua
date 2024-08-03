local channel = require "coutil.channel"

local id<const> = ...
local histlen<const> = 8
local buffer<const> = memory.create(8192)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")

repeat
	assert(system.awaitch(startch, "in"))
	local histogram<const> = setmetatable({}, {__index = function () return 0 end})
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % histlen)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(string.char(64+id)); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram, 1, histlen)))
until false
