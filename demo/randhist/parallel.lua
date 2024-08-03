local vararg = require "vararg"
local channel = require "coutil.channel"
local threads = require "coutil.threads"

local repeats<const> = 100

local ncpu<const> = #system.cpuinfo()
local pool<const> = threads.create(ncpu)
local console<const> = arg[-1]
local worker<const> = arg[0]:gsub("parallel%.lua$", "worker.lua")
for i = 1, ncpu do
	assert(pool:dofile(console, "t", worker, i))
end

spawn.call(function ()
	local histoch<close> = channel.create("histogram")
	local histogram<const> = setmetatable({}, {__index = function () return 0 end})
	for i = 1, repeats do
		local partial = vararg.pack(select(2, assert(system.awaitch(histoch, "in"))))
		io.write("+"); io.flush()
		for pos, count in partial do
			histogram[pos] = histogram[pos] + count
		end
	end
	pool:resize(0)
	print()
	print(table.unpack(histogram, 1, histlen))
end)

local startch<close> = channel.create("start")
for i = 1, repeats do
	io.write("*"); io.flush()
	assert(system.awaitch(startch, "out"))
end
io.write("."); io.flush()
