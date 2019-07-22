host = select(1, ...) or "ip6-localhost"
port = tonumber(select(2, ...) or 54321)

szlen = 2
szfmt = "I"..szlen
maxlen = 8192
maxtime = 30

local function panic(errmsg)
	local system = require "coutil.system"
	io.stderr:write(debug.traceback(errmsg), "\n")
	if system.isrunning() then
		system.halt()
	end
end

function spawn(f, ...)
	local pspawn = require "coutil.spawn"
	return pspawn.catch(panic, f, ...)
end

function log(...)
	io.write(...)
	io.flush()
end