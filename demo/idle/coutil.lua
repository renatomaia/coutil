dofile "configs.lua"

local system<const> = require "coutil.system"

coroutine.resume(coroutine.create(function ()
	for i = 1, repeats do
		system.suspend()
	end
end))

system.run()
