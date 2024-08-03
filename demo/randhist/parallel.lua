local threads = require "coutil.threads"

local ncpu<const> = #system.cpuinfo()
local pool<const> = threads.create(ncpu)
local console<const> = arg[-1]
local worker<const> = arg[0]:gsub("parallel%.lua$", "worker.lua")
for i = 1, ncpu do
	assert(pool:dofile(console, "t", "-W", worker, i))
end

local channel = require "coutil.channel"

local repeats<const> = 100

spawn.call(function ()
	local histoch<close> = channel.create("histogram")
	local histogram<const> = {}
	for i = 1, repeats do
		local partial<const> = { select(2, assert(system.awaitch(histoch, "in"))) }
		for pos, count in ipairs(partial) do
			histogram[pos] = (histogram[pos] or 0) + count
		end
		io.write("+"); io.flush()
	end
	pool:resize(0)
	print()
	print(table.unpack(histogram))
end)

local startch<close> = channel.create("start")
for i = 1, repeats do
	io.write("."); io.flush()
	assert(system.awaitch(startch, "out"))
end
io.write("!"); io.flush()
