local _ENV = require "_G"
local io = require "io"
local string = require "string"
local table = require "table"
local memory = require "memory"
local channel = require "coutil.channel"
local spawn = require "coutil.spawn"
local system = require "coutil.system"

spawn.catch(print, function (...)

	local i<const> = ...
	local name<const> = string.char(string.byte("A") + i - 1)
	local buffer<const> = memory.create(131072)
	local startch<close> = channel.create("start")
	local histoch<close> = channel.create("histogram")

	while true do
		assert(system.awaitch(startch, "in"))
		local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
		system.random(buffer)
		for j = 1, #buffer do
			local pos = 1 + (buffer:get(j) % #histogram)
			histogram[pos] = histogram[pos] + 1
		end
		io.write(name); io.flush()
		assert(system.awaitch(histoch, "out", table.unpack(histogram)))
	end

end, ...)

system.run()
