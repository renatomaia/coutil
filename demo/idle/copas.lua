dofile "configs.lua"

local copas = require "copas"

copas.addthread(function ()
	for i = 1, repeats do
		copas.sleep(0)
	end
end)

copas.loop()
