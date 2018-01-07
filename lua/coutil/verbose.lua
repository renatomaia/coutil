local Viewer = require "loop.debug.Viewer"
local Verbose = require "loop.debug.Verbose"

local viewer = Viewer{
	linebreak = false,
	nolabels = true,
	noindices = true,
	metaonly = true,
	maxdepth = 2,
}
local log = Verbose{ viewer = viewer }

log:settimeformat("%H:%M:%S")
log.timed = true
log:flag("debug", true)

return log
