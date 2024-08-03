local channel = require "coutil.channel"

local buffer<const> = memory.create(8192)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")
local workerid<const> = ...

repeat
	assert(system.awaitch(startch, "in"))
	local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % #histogram)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(string.char(string.byte("A") + workerid - 1)); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram)))
until false
