dofile "configs.lua"

local uv = require "luv"

local timer = uv.new_idle()
timer:start(function ()
	repeats = repeats-1
	if repeats == 0 then
		timer:stop()
	end
end)

uv.run()
