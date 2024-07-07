local system = require "coutil.system"

function testobject(create, ...)
	do case "garbage generation"
		garbage.sock = assert(create(...))

		done()
	end

	do case "garbage collection"
		garbage.sock = assert(create(...))

		gc()
		assert(garbage.sock == nil)

		assert(system.run() == false)

		done()
	end

	do case "close"
		garbage.sock = assert(create(...))

		assert(garbage.sock:close() == true)

		asserterr("closed object", pcall(garbage.sock.getaddress, garbage.sock))

		asserterr("closed object", pcall(garbage.sock.close, garbage.sock))

		gc()
		assert(garbage.sock ~= nil)

		assert(system.run() == false)

		done()
	end
end

function teststream(create, addresses)

	newtest "accept"

	local backlog = 3

	do case "errors"
		local passive = assert(create("passive"))

		assert(passive.connect == nil)
		assert(passive.write == nil)
		assert(passive.read == nil)
		assert(passive.shutdown == nil)

		asserterr("not listening", pspawn(passive.accept, passive))
		asserterr("number expected, got no value", pcall(passive.listen, passive))
		asserterr("out of range", pcall(passive.listen, passive, -1))
		asserterr("out of range", pcall(passive.listen, passive, 1<<31))
		assert(passive:bind(addresses.bindable))
		assert(passive:listen(backlog))
		asserterr("already listening", pcall(passive.listen, passive, backlog))
		asserterr("unable to yield", pcall(passive.accept, passive))
		assert(passive:close())

		done()
	end

	do case "canceled listen"
		do
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
		end
		gc()

		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))
		assert(server:close())

		local collectable = assert(create("passive"))
		assert(collectable:bind(addresses.bindable))
		assert(collectable:listen(backlog))
		garbage.server = collectable
		collectable = nil

		done()
	end

	do case "successful connections"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			for i = 1, 3 do
				local stream = assert(server:accept())
				assert(stream:close())
				stage = i
			end
		end)
		assert(stage == 0)

		local connected = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(create("stream"))
				assert(stream:connect(addresses.bindable))
				assert(stream:close())
				connected[i] = true
			end)
		end
		assert(stage == 0)
		for i = 1, 3 do
			assert(connected[i] == nil)
		end

		gc()
		assert(system.run() == false)
		assert(stage == 3)
		for i = 1, 3 do
			assert(connected[i] == true)
		end
		assert(server:close())

		done()
	end

	do case "accept transfer"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local thread
		spawn(function ()
			local stream = assert(server:accept())
			assert(stream:close())
			coroutine.resume(thread)
		end)

		local complete = false
		spawn(function ()
			asserterr("already in use", pcall(server.accept, server))
			thread = coroutine.running()
			coroutine.yield()
			local stream = assert(server:accept())
			assert(stream:close())
			complete = true
		end)

		spawn(function ()
			for i = 1, 2 do
				local stream = assert(create("stream"))
				assert(stream:connect(addresses.bindable))
				assert(stream:close())
			end
		end)

		gc()
		assert(complete == false)
		assert(system.run() == false)
		assert(complete == true)
		assert(server:close())
		thread = nil

		done()
	end

	do case "accumulated connects"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(create("stream"))
				stage[i] = 0
				assert(stream:connect(addresses.bindable))
				stage[i] = 1
				assert(stream:close())
				stage[i] = 2
			end)
			assert(stage[i] == 0)
		end

		local accepted = false
		spawn(function ()
			for i = 1, 3 do
				local stream = assert(server:accept())
				assert(stream:close())
			end
			assert(server:close())
			accepted = true
		end)
		assert(accepted == false)

		gc()
		assert(system.run() == false)
		assert(accepted == true)
		for i = 1, 3 do
			assert(stage[i] == 2)
		end

		done()
	end

	do case "accumulated transfers"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = {}
		for i = 1, 3 do
			spawn(function ()
				local stream = assert(create("stream"))
				stage[i] = 0
				assert(stream:connect(addresses.bindable))
				stage[i] = 1
				assert(stream:close())
				stage[i] = 2
			end)
			assert(stage[i] == 0)
		end

		local accepted = false
		local function acceptor(count)
			if count > 0 then
				local stream = assert(server:accept())
				assert(stream:close())
				spawn(acceptor, count-1)
			else
				assert(server:close())
				accepted = true
			end
		end
		spawn(acceptor, 3)
		assert(accepted == false)

		gc()
		assert(system.run() == false)
		assert(accepted == true)
		for i = 1, 3 do
			assert(stage[i] == 2)
		end

		done()
	end

	do case "different accepts"
		local complete = false
		spawn(function ()
			local server1 = assert(create("passive"))
			assert(server1:bind(addresses.bindable))
			assert(server1:listen(backlog))

			local server2 = assert(create("passive"))
			assert(server2:bind(addresses.free))
			assert(server2:listen(backlog))

			local stream = assert(server1:accept())
			assert(stream:close())

			local stream = assert(server2:accept())
			assert(stream:close())

			assert(server1:close())
			assert(server2:close())
			complete = true
		end)
		assert(complete == false)

		for _, address in ipairs{addresses.bindable, addresses.free} do
			spawn(function ()
				local stream = assert(create("stream"))
				assert(stream:connect(address))
				assert(stream:close())
			end)
		end

		gc()
		assert(system.run() == false)
		assert(complete == true)

		done()
	end

	do case "close in accept"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage1 = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			asserterr("closed", server:accept())
			stage1 = 1
		end)
		assert(stage1 == 0)

		local stage2 = 0
		spawn(function ()
			system.suspend()
			stage2 = 1
			assert(server:close())
			stage2 = 2
		end)
		assert(stage2 == 0)

		gc()
		assert(system.run() == false)
		assert(stage1 == 1)
		assert(stage2 == 2)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			local stream, a,b,c = server:accept()
			assert(stream == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro, garbage, true,nil,3)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel and reschedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream, extra = server:accept()
			assert(stream == nil)
			assert(extra == nil)
			stage = 2
			local stream = assert(server:accept())
			assert(stream:close())
			stage = 3
		end)
		assert(stage == 1)

		coroutine.resume(garbage.coro)
		assert(stage == 2)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			assert(stream:close())
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 3)

		done()
	end

	do case "double cancel"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream, extra = server:accept()
			assert(stream == nil)
			assert(extra == nil)
			stage = 2
			local a,b,c = server:accept()
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 3
		end)
		assert(stage == 1)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3)
			assert(stage == 3)
			stage = 4
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 2)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "ignore errors"

		local stage = 0
		pspawn(function ()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:close())
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			assert(stream:close())
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "ignore errors after cancel"

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			assert(server:accept() == garbage)
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		coroutine.resume(garbage.coro, garbage)
		assert(stage == 2)

		gc()
		assert(system.run() == false)

		done()
	end

	newtest "connect"

	do case "errors"
		local stream = assert(create("stream"))

		assert(stream.passive == nil)
		assert(stream.accept == nil)

		local stage = 0
		spawn(function ()
			system.suspend()
			local stream = assert(create("stream"))
			stage = 1
			asserterr("expected, got no value", pcall(stream.connect, stream))
			stage = 2
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "successful connection"
		local connected
		spawn(function ()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			connected = false
			assert(server:accept())
			connected = true
		end)
		assert(connected == false)

		local stage = 0
		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			stage = 1
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
		assert(connected == true)

		done()
	end

	if addresses.denied then case "connection refused"
		local stage = 0
		spawn(function ()
			local stream = assert(create("stream"))
			asserterr("connection refused", stream:connect(addresses.denied))
			assert(stream:close())
			stage = 1
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel schedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(create("stream"))
			local a,b,c = stream:connect(addresses.bindable)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			stream:close()
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro, true,nil,3)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel then collected"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			garbage.stream = assert(create("stream"))
			local a,b,c = garbage.stream:connect(addresses.bindable)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro, true,nil,3)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "cancel and reschedule"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(create("stream"))
			local a,b = stream:connect(addresses.bindable)
			assert(a == nil)
			assert(b == nil)
			stage = 1
			a,b = stream:connect(addresses.bindable)
			assert(a == true or b == "socket is already connected")
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		local connected
		spawn(function ()
			assert(server:accept())
			connected = true
		end)
		assert(connected == nil)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		server:close()

		done()
	end

	do case "resume while closing"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable) == nil)
			stage = 1
			local a,b,c = stream:connect(addresses.bindable)
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 2
		end)
		assert(stage == 0)

		spawn(function ()
			system.suspend()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
			assert(stage == 2)
			stage = 3
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 3)
		assert(server:close())

		done()
	end

	do case "ignore errors"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			stage = 1
			error("oops!")
		end)
		assert(stage == 0)

		spawn(function ()
			system.suspend()
			assert(server:accept())
			assert(server:close())
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 1)

		done()
	end

	do case "ignore errors after cancel"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable) == nil)
			stage = 1
			error("oops!")
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
		assert(server:close())

		done()
	end

	newtest "receive"

	local memory = require "memory"

	local backlog = 3

if standard == "posix" then
	do case "errors"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local buffer = memory.create(64)

		local brokenpipe
		spawn(function ()
			brokenpipe = coroutine.running()
			system.awaitsig("brokenpipe")
			brokenpipe = nil
		end)

		local stage1 = 0
		local accepted, stream
		spawn(function ()
			stage1 = 1
			accepted = assert(server:accept())
			stage1 = 2
			asserterr("memory expected", pcall(accepted.read, accepted))
			asserterr("number has no integer representation",
				pcall(accepted.read, accepted, buffer, 1.1))
			asserterr("number has no integer representation",
				pcall(accepted.read, accepted, buffer, nil, 2.2))
			stage1 = 3
			assert(accepted:read(buffer) == 64)
			stage1 = 4
			assert(stream:close() == true)
			stage1 = 5
			local res, err = accepted:write(string.rep("x", 1<<24))
			assert(res == false)
			assert(err == "operation canceled" or err == "broken pipe")
			stage1 = 6
		end)
		assert(stage1 == 1)
		local stage2 = 0
		spawn(function ()
			stream = assert(create("stream"))
			stage2 = 1
			assert(stream:connect(addresses.bindable))
			stage2 = 2
			asserterr("already in use", pcall(accepted.read, accepted, buffer))
			stage2 = 3
			assert(stream:write(string.rep("x", 64)) == true)
			stage2 = 4
			asserterr("closed", stream:read(buffer))
			stage2 = 5
			system.suspend()
			assert(accepted:close() == true)
			system.suspend()
			stage2 = 6
			if brokenpipe then coroutine.resume(brokenpipe) end
		end)
		assert(stage2 == 1)
		assert(system.run("step") == true)
		assert(stage1 == 2)
		assert(stage2 == 3 or stage2 == 4)

		asserterr("already in use", pcall(accepted.read, accepted, buffer))

		gc()
		assert(system.run() == false)
		assert(stage1 == 6)
		assert(stage2 == 6)

		assert(server:close() == true)

		done()
	end
end

	do case "end of transmission"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local data1 = string.rep("a", 64)
		local data2 = string.rep("b", 64)

		local done1
		spawn(function ()
			local stream = assert(server:accept())
			assert(server:close())
			local buffer = memory.create(128)
			assert(stream:read(buffer) == 64)
			assert(memory.tostring(buffer, 1, 64) == data1)
			asserterr("end of file", stream:read(buffer))
			if standard == "win32" then -- TODO: check if this is a bug in libuv
				asserterr("socket is not connected", stream:read(buffer))
				if tostring(stream):find("pipe") then
					asserterr("broken pipe", stream:write(data2))
				else
					assert(stream:write(data2))
				end
			else
				asserterr("end of file", stream:read(buffer))
				assert(stream:write(data2))
			end
			done1 = true
		end)
		assert(done1 == nil)

		local done2
		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			assert(stream:write(data1))
			system.suspend()
			assert(stream:shutdown())
			local buffer = memory.create(128)
			if standard == "win32" and tostring(stream):find("pipe") then -- TODO: check if this is a bug in libuv
				asserterr("end of file", stream:read(buffer))
				assert(memory.tostring(buffer) == string.rep("\0", 128))
			else
				assert(stream:read(buffer))
				assert(memory.tostring(buffer, 1, 64) == data2)
			end
			done2 = true
		end)
		assert(done2 == nil)

		gc()
		assert(system.run() == false)
		assert(done1 == true)
		assert(done2 == true)

		done()
	end

	do case "successful transmissions"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local data = string.rep("a", 64)
		           ..string.rep("b", 32)
		           ..string.rep("c", 32)

		local function assertfilled(buffer, count)
			local expected = string.sub(data, 1, count)..string.rep("\0", #data-count)
			assert(memory.diff(buffer, expected) == nil)
		end

		local done1
		spawn(function ()
			local stream = assert(server:accept())
			assert(server:close())
			local buffer = memory.create(128)
			assert(stream:read(buffer) == 64)
			assertfilled(buffer, 64)
			done1 = false
			assert(stream:read(buffer, 65, 96) == 32)
			assertfilled(buffer, 96)
			assert(stream:read(buffer, 97) == 32)
			assertfilled(buffer, 128)
			done1 = true
		end)
		assert(done1 == nil)

		local done2
		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			assert(stream:write(data, 1, 64))
			while done1 == nil do system.suspend() end
			assert(stream:write(data, 65, 128))
			assert(stream:close())
			done2 = true
		end)
		assert(done2 == nil)

		gc()
		assert(system.run() == false)
		assert(done1 == true)
		assert(done2 == true)

		done()
	end

	do case "receive transfer"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local size = 32
		local buffer = memory.create(size)

		local stream, thread
		spawn(function ()
			stream = assert(server:accept())
			assert(server:close())
			assert(stream:read(buffer) == size)
			assert(not memory.diff(buffer, string.rep("a", size)))
			coroutine.resume(thread)
			gc()
		end)

		local complete = false
		spawn(function ()
			thread = coroutine.running()
			coroutine.yield()
			if standard == "posix" then
				asserterr("already in use", pcall(stream.read, stream))
				coroutine.yield()
			end
			thread = nil
			assert(stream:read(buffer) == size)
			assert(not memory.diff(buffer, string.rep("b", size)))
			assert(stream:close())
			complete = true
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			if standard == "posix" then
				coroutine.resume(thread)
			end
			assert(stream:write(string.rep("a", size)))
			assert(stream:write(string.rep("b", size)))
			assert(stream:close())
		end)

		gc()
		assert(complete == false)
		assert(system.run() == false)
		assert(complete == true)

		done()
	end

	do case "multiple threads"
		local complete = {}
		for i, address in ipairs{addresses.bindable, addresses.free} do
			spawn(function ()
				local server = assert(create("passive"))
				assert(server:bind(address))
				assert(server:listen(backlog))

				local stream = assert(server:accept())
				spawn(function ()
					local buffer = memory.create(64)
					assert(stream:read(buffer) == 32)
					memory.fill(buffer, buffer, 33, 64)
					assert(stream:write(buffer))
					assert(stream:close())
				end)

				assert(server:close())
			end)

			spawn(function ()
				local stream = assert(create("stream"))
				assert(stream:connect(address))
				assert(stream:write(string.rep("x", 32)))
				local buffer = memory.create(64)
				assert(stream:read(buffer) == 64)
				assert(stream:close())
				assert(not memory.diff(buffer, string.rep("x", 64)))
				complete[i] = true
			end)
			assert(complete[i] == nil)
		end

		gc()
		assert(system.run() == false)
		assert(complete[1] == true)
		assert(complete[2] == true)

		done()
	end

	do case "cancel schedule"

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
			stage = 1
			local res, a,b,c = stream:read(memory.create(10))
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 2
			stream:close()
			coroutine.yield()
			stage = 3
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 1
			coroutine.resume(garbage.coro, garbage, true,nil,3)
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "cancel and reschedule"
		local stage
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 0
			local stream = assert(server:accept())
			stage = 1
			local buffer = memory.create("9876543210")
			local bytes, extra = stream:read(buffer)
			assert(bytes == nil)
			assert(extra == nil)
			assert(not memory.diff(buffer, "9876543210"))
			stage = 2
			assert(stream:read(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			assert(stream:close())
			stage = 4
		end)
		assert(stage == 0)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 1
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			assert(stream:write("0123456789"))
			stage = 3
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "double cancel"

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
			stage = 1
			local buffer = memory.create("9876543210")
			local bytes, extra = stream:read(buffer)
			assert(bytes == nil)
			assert(extra == nil)
			stage = 2
			local a,b,c = stream:read(buffer)
			assert(a == .1)
			assert(b == 2.2)
			assert(c == 33.3)
			stage = 3
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 1
			coroutine.resume(garbage.coro)
			assert(stage == 2)
			system.suspend()
			coroutine.resume(garbage.coro, .1, 2.2, 33.3)
			assert(stage == 3)
			stage = 4
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "ignore errors"

		local stage = 0
		pspawn(function ()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			local buffer = memory.create("9876543210")
			assert(stream:read(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			stage = 2
			error("oops!")
		end)
		assert(stage == 1)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			assert(stream:write("0123456789"))
		end)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "ignore errors after cancel"

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			stage = 2
			assert(stream:read(memory.create(10)) == garbage)
			stage = 3
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 2
			coroutine.resume(garbage.coro, garbage)
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 3)

		done()
	end

	newtest "send"

if standard == "posix" then
	do case "errors"
		local server = assert(create("passive"))
		assert(server:bind(addresses.bindable))
		assert(server:listen(backlog))

		local data = string.rep("x", 1<<24)

		local brokenpipe
		spawn(function ()
			brokenpipe = coroutine.running()
			system.awaitsig("brokenpipe")
			brokenpipe = nil
		end)

		local stage1 = 0
		local accepted, stream
		spawn(function ()
			stage1 = 1
			accepted = assert(server:accept())
			assert(server:close() == true)
			stage1 = 2
			asserterr("memory expected", pcall(accepted.write, accepted))
			asserterr("number has no integer representation",
				pcall(accepted.write, accepted, data, 1.1))
			asserterr("number has no integer representation",
				pcall(accepted.write, accepted, data, nil, 2.2))
			stage1 = 3
			local res, err = accepted:write(data)
			assert(res == false)
			assert(err == "connection reset by peer" or err == "broken pipe")
			stage1 = 4
			coroutine.resume(brokenpipe)
		end)
		assert(stage1 == 1)

		local stage2 = 0
		spawn(function ()
			stream = assert(create("stream"))
			stage2 = 1
			assert(stream:connect(addresses.bindable))
			stage2 = 2
			system.suspend()
			stage2 = 3
			assert(stream:close())
			stage2 = 4
		end)
		assert(stage2 == 1)

		assert(system.run("step") == true)
		assert(stage1 == 3)
		assert(stage2 == 2)

		asserterr("unable to yield", pcall(accepted.write, accepted, buffer))

		gc()
		assert(system.run() == false)
		assert(stage1 == 4)
		assert(stage2 == 4)

		done()
	end
end

	do case "cancel schedule"

		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			local stream = assert(server:accept())
			stage = 1
			local res, a,b,c = stream:write(string.rep("x", 1<<24))
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 2
			stream:close()
			coroutine.yield()
			stage = 3
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 1
			coroutine.resume(garbage.coro, garbage, true,nil,3)

			local bytes = 1<<24
			local buffer = memory.create(1<<24)
			repeat
				local read, errmsg = stream:read(buffer)
				if not read and errmsg == "end of file" then
					break
				end
				bytes = bytes-read
			until bytes <= 0
		end)
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end

	do case "cancel and reschedule"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			stage = 2
			local ok, extra = stream:write(string.rep("x", 1<<24))
			assert(ok == nil)
			assert(extra == nil)
			stage = 3
			assert(stream:write(string.rep("x", 64)) == true)
			stage = 4
			assert(stream:close())
			stage = 5
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 2
			coroutine.resume(garbage.coro)
			local bytes = (1<<24)+64
			local buffer = memory.create(bytes)
			repeat
				local read, errmsg = stream:read(buffer)
				if not read and errmsg == "end of file" then
					break
				end
				bytes = bytes-read
			until bytes <= 0
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 5)

		done()
	end

	if standard == "posix" then case "double cancel"
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			local ok, extra = stream:write(string.rep("x", 1<<24))
			assert(ok == nil)
			assert(extra == nil)
			stage = 3
			local res, a,b,c = stream:write(string.rep("x", 1<<24))
			assert(res == garbage)
			assert(a == true)
			assert(b == nil)
			assert(c == 3)
			stage = 3

			assert(stream:close())
			stage = 4
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			system.suspend()
			coroutine.resume(garbage.coro)
			local bytes = 1<<24
			local buffer = memory.create(1<<24)
			repeat
				bytes = bytes-stream:read(buffer)
			until bytes <= 0
			coroutine.resume(garbage.coro, garbage, true,nil,3)
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

	do case "ignore errors"

		local stage = 0
		pspawn(function ()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			stage = 2
			assert(stream:write("0123456789") == true)
			stage = 3
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			repeat system.suspend() until stage >= 2
			local buffer = memory.create("9876543210")
			assert(stream:read(buffer) == 10)
			assert(not memory.diff(buffer, "0123456789"))
			assert(stage == 3)
			stage = 4
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 4)

		done()
	end

if standard == "posix" then
	do case "ignore errors after cancel"

		local brokenpipe
		spawn(function ()
			brokenpipe = coroutine.running()
			system.awaitsig("brokenpipe")
			brokenpipe = nil
		end)

		local stage = 0
		pspawn(function ()
			garbage.coro = coroutine.running()
			local server = assert(create("passive"))
			assert(server:bind(addresses.bindable))
			assert(server:listen(backlog))
			stage = 1
			local stream = assert(server:accept())
			assert(stream:write(string.rep("x", 1<<24)) == garbage)
			stage = 2
			error("oops!")
		end)

		spawn(function ()
			local stream = assert(create("stream"))
			assert(stream:connect(addresses.bindable))
			system.suspend()
			coroutine.resume(garbage.coro, garbage)

			assert(stream:close())

			system.suspend()
			coroutine.resume(brokenpipe)
		end)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)

		done()
	end
end

end