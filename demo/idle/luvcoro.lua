dofile "configs.lua"

local uv = require "luv"

local yield = coroutine.yield
local counter = coroutine.create(function ()
	for i = 1, repeats do
		yield(true)
	end
end)

local resume = coroutine.resume
local timer = uv.new_idle()
timer:start(function ()
	local _, cont = resume(counter)
	if not cont then
		timer:stop()
	end
end)

uv.run()
