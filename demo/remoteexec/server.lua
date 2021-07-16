dofile "utils.lua"

local memory = require "memory"
local system = require "coutil.system"

local function timedresume(timeout, thread)
	if system.suspend(timeout) then
		coroutine.resume(thread, nil, "timeout")
	end
end

local function unpackcmd(buffer)
	local command = {
		execfile = nil,
		arguments = {},
	}
	local size, index = memory.unpack(buffer, szfmt, szlen+1)
	command.execfile, index = memory.unpack(buffer, "z", index)
	for i = 1, size do
		command.arguments[i], index = memory.unpack(buffer, "z", index)
	end
	return command
end

local function readbytes(conn, buffer, bytes, expected)
	while bytes < expected do
		local received, errmsg = conn:receive(buffer, bytes+1)
		if received == nil then
			return nil, errmsg
		end
		bytes = bytes+received
	end
	return bytes
end

local function doconn(conn)
	log("new client request\n")
	local timeout = spawn(timedresume, maxtime, coroutine.running())
	local buffer = memory.create(szlen+maxlen)
	local res, errmsg = readbytes(conn, buffer, 0, szlen)
	if res ~= nil then
		local size, index = memory.unpack(buffer, szfmt)
		if size <= maxlen then
			res, errmsg = readbytes(conn, buffer, res, szlen+size)
			if res ~= nil then
				local ok, command = pcall(unpackcmd, buffer)
				if ok then
					log("executing command ",command.execfile,"\n")
					res, errmsg = system.execute(command)
					if res ~= nil then
						res = string.format("%s(%s)", res, errmsg)
					elseif errmsg == "timeout" then
						log("terminating command ",command.execfile,"\n")
						system.emitsig(command.pid, "terminate")
					end
				elseif string.find(command, "data too short", 1, "noregex") ~= nil then
					errmsg = "malformed message"
				else
					error(command)
				end
			end
		else
			errmsg = "out of memory"
		end
	end
	if errmsg ~= "timeout" then
		coroutine.resume(timeout, nil, "terminate")
	end
	local _, index = memory.pack(buffer, "s"..szlen, 1, res or errmsg)
	conn:send(buffer, 1, index-1)
	conn:shutdown()
	conn:close()
	log("client request complete (res=",res or errmsg,")\n")
end

spawn(function ()
	log("starting server ... ")
	local address = assert(system.findaddr("*", port, "s6"))()
	local server = assert(system.socket("passive", address.type))
	assert(server:bind(address))
	assert(server:listen(32))
	spawn(function (thread)
		if system.awaitsig("interrupt") == "interrupt" then
			coroutine.resume(thread, nil, "terminate")
			log("terminating server ... ")
		end
	end, coroutine.running())
	log("started\n")
	repeat
		local conn, errmsg = server:accept()
		if conn ~= nil then
			spawn(doconn, conn)
		elseif errmsg ~= "terminate" then
			error(errmsg)
		end
	until conn == nil
	server:close()
end)

system.run()

log("terminated\n")
