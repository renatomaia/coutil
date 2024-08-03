local channel = require "coutil.channel"
local system = require "coutil.system"

local i<const> = ...
local histlen<const> = 8
local bufsz<const> = 8192
local buffer<const> = memory.create(bufsz)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")

repeat
	assert(system.awaitch(startch, "in"))
	local histogram<const> = setmetatable({}, {__index = function () return 0 end})
	system.random(buffer)
	for j = 1, bufsz do
		local pos = 1 + (buffer:get(j) % histlen)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(string.char(64+i)); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram, 1, histlen)))
until false
