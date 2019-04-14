local system = require "coutil.system"

newtest "run" ------------------------------------------------------------------

do case "error messages"
	asserterr("invalid option", pcall(system.run, "none"))
	asserterr("string expected", pcall(system.run, coroutine.running()))

	done()
end

do case "empty call"
	assert(system.run() == false)

	done()
end

do case "nested call"
	local stage = 0
	spawn(function ()
		system.pause()
		asserterr("already running", pcall(system.run))
		stage = 1
	end)
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "run step|ready"
	for _, mode in ipairs{"step", "ready"} do
		local n = 3
		local stage = {}
		for i = 1, n do
			stage[i] = 0
			spawn(function (c)
				for j = 1, c do
					system.pause()
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		for i = 1, n do
			gc()
			assert(system.run(mode) == (i < n))
			for j = 1, n do
				assert(stage[j] == (j < i and j or i))
			end
		end

		gc()
		assert(system.run() == false)
		for i = 1, n do
			assert(stage[i] == i)
		end
	end

	done()
end

do case "run loop"
	local mode = "loop"
	do ::again::
		local n = 3
		local stage = {}
		for i = 1, n do
			stage[i] = 0
			spawn(function (c)
				for j = 1, c do
					system.pause()
					stage[i] = j
				end
			end, i)
			assert(stage[i] == 0)
		end

		gc()
		assert(system.run("loop") == false)
		for i = 1, n do
			assert(stage[i] == i)
		end
		if mode == "loop" then
			mode = nil
			goto again
		end
	end

	done()
end

newtest "pause" ----------------------------------------------------------------

do case "error messages"
	asserterr("number expected", pcall(system.pause, false))
	asserterr("unable to yield", pcall(system.pause))

	done()
end

local args = { nil, 0, 0.1 }

do case "yield values"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		local a,b,c = spawn(function ()
			local res, extra = system.pause(delay, "testing", 1, 2, 3)
			assert(res == true)
			assert(extra == nil)
			stage = 1
		end)
		assert(a == nil)
		assert(b == nil)
		assert(c == nil)
		assert(stage == 0)
		gc()

		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "scheduled yield"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			system.pause(delay)
			stage = 1
			coroutine.yield()
			stage = 2
		end)
		assert(stage == 0)
		gc()

		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			local res, extra = system.pause(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 1
			local res, extra = system.pause(delay)
			assert(res == true)
			assert(extra == nil)
			stage = 2
		end)
		assert(stage == 0)

		gc()
		assert(system.run("step") == true)
		assert(stage == 1)

		gc()
		if delay ~= nil and delay > 0 then
			assert(system.run("ready") == true)
		else
			assert(system.run("step") == false)
		end

		gc()
		assert(system.run() == false)
		assert(stage == 2)
	end

	done()
end

do case "cancel schedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local a,b,c = system.pause(delay)
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
	end

	done()
end

do case "cancel and reschedule"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			local extra = system.pause(delay)
			assert(extra == nil)
			stage = 1
			assert(system.pause(delay) == true)
			stage = 2
		end)
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 2)
	end

	done()
end

do case "resume while closing"
	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		spawn(function ()
			garbage.coro = coroutine.running()
			assert(system.pause(delay) == nil)
			stage = 1
			local a,b,c = system.pause(delay)
			assert(a == 1)
			assert(b == 22)
			assert(c == 333)
			stage = 2
		end)
		assert(stage == 0)

		spawn(function ()
			system.pause()
			coroutine.resume(garbage.coro, 1,22,333) -- while being closed.
			assert(stage == 2)
			stage = 3
		end)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 3)
	end

	done()
end

do case "ignore errors"

	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		pspawn(function (errmsg)
			system.pause(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

do case "ignore errors after cancel"

	for i = 1, 3 do
		local delay = args[i]
		local stage = 0
		pspawn(function (errmsg)
			garbage.coro = coroutine.running()
			system.pause(delay)
			stage = 1
			error(errmsg)
		end, "oops!")
		assert(stage == 0)

		coroutine.resume(garbage.coro)
		assert(stage == 1)

		gc()
		assert(system.run() == false)
		assert(stage == 1)
	end

	done()
end

newtest "awaitsig" -------------------------------------------------------------

local function sendsignal(name)
	os.execute("killall -"..name.." lua")
end

do case "error messages"
	asserterr("unable to yield", pcall(system.awaitsig, "user1"))
	asserterr("unable to yield", pcall(system.awaitsig, "kill"))
	asserterr("invalid signal", pspawn(system.awaitsig, "kill"))

	done()
end

do case "yield values"
	local stage = 0
	local a,b,c = spawn(function ()
		local res, extra = system.awaitsig("user1", "testing", 1, 2, 3)
		assert(res == "user1")
		assert(extra == nil)
		stage = 1
	end)
	assert(a == nil)
	assert(b == nil)
	assert(c == nil)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		sendsignal("USR1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "scheduled yield"
	local stage = 0
	spawn(function ()
		system.awaitsig("user1")
		stage = 1
		coroutine.yield()
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		sendsignal("USR1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "reschedule same signal"
	local stage = 0
	spawn(function ()
		system.awaitsig("user1")
		stage = 1
		system.awaitsig("user1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		sendsignal("USR1")
		system.pause()
		sendsignal("USR1")
	end)

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "reschedule different signal"
	local stage = 0
	spawn(function ()
		system.awaitsig("user1")
		stage = 1
		system.awaitsig("user2")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		sendsignal("USR1")
		system.pause()
		sendsignal("USR2")
	end)

	gc()
	assert(system.run("step") == true)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "cancel schedule"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local a,b,c = system.awaitsig("user1")
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
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		local extra = system.awaitsig("user1")
		assert(extra == nil)
		stage = 1
		assert(system.awaitsig("user1") == "user1")
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause() -- the first signal handle is active.
		system.pause() -- the first signal handle is being closed.
		sendsignal("USR1") -- the second signal handle is active.
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 2)

	done()
end

do case "resume while closing"
	local stage = 0
	spawn(function ()
		garbage.coro = coroutine.running()
		assert(system.awaitsig("user1") == nil)
		stage = 1
		local a,b,c = system.awaitsig("user1")
		assert(a == .1)
		assert(b == 2.2)
		assert(c == 33.3)
		stage = 2
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		coroutine.resume(garbage.coro, .1, 2.2, 33.3) -- while being closed.
		assert(stage == 2)
		stage = 3
	end)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 3)

	done()
end

do case "ignore errors"

	local stage = 0
	pspawn(function ()
		assert(system.awaitsig("user1"))
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	spawn(function ()
		system.pause()
		sendsignal("USR1")
	end)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

do case "ignore errors after cancel"

	local stage = 0
	pspawn(function ()
		garbage.coro = coroutine.running()
		system.awaitsig("user1")
		stage = 1
		error("oops!")
	end)
	assert(stage == 0)

	coroutine.resume(garbage.coro)
	assert(stage == 1)

	gc()
	assert(system.run() == false)
	assert(stage == 1)

	done()
end

newtest "address" --------------------------------------------------------------

do case "empty ipv4"
	local a = system.address("ipv4")

	assert(tostring(a) == "0.0.0.0:0")
	assert(a.type == "ipv4")
	assert(a.port == 0)
	assert(a.literal == "0.0.0.0")
	assert(a.binary == "\0\0\0\0")
	assert(a == system.address("ipv4"))

	done()
end

do case "empty ipv6"
	local a = system.address("ipv6")

	assert(tostring(a) == "[::]:0")
	assert(a.type == "ipv6")
	assert(a.port == 0)
	assert(a.literal == "::")
	assert(a.binary == string.rep("\0", 16))
	assert(a == system.address("ipv6"))

	done()
end

local cases = {
	ipv4 = {
		port = 8080,
		literal = "192.168.0.1",
		binary = "\192\168\000\001",
		uri = "192.168.0.1:8080",
		changes = {
			port = 54321,
			literal = "127.0.0.1",
			binary = "byte",
		},
		equivalents = {
			["10.20.30.40"] = "\10\20\30\40",
			["40.30.20.10"] = "\40\30\20\10",
		},
	},
	ipv6 = {
		port = 8888,
		literal = "::ffff:192.168.0.1",
		binary = "\0\0\0\0\0\0\0\0\0\0\xff\xff\192\168\000\001",
		uri = "[::ffff:192.168.0.1]:8888",
		changes = {
			port = 12345,
			literal = "::1",
			binary = "bytebytebytebyte",
		},
		equivalents = {
			["1::f"] =
				"\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\x0f",
			["1:203:405:607:809:a0b:c0d:e0f"] =
				"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f",
			["123:4567:89ab:cdef:fedc:ba98:7654:3210"] =
				"\x01\x23\x45\x67\x89\xab\xcd\xef\xfe\xdc\xba\x98\x76\x54\x32\x10",
		},
	},
}
for type, expected in pairs(cases) do
	local addr = system.address(type, expected.uri)

	local function checkaddr(a)
		--assert(system.type(a) == "address")
		assert(tostring(a) == expected.uri)
		assert(a.type == type)
		assert(a.port == expected.port)
		assert(a.literal == expected.literal)
		assert(a.binary == expected.binary)
		assert(a == addr)
	end

	do case("create "..type)
		checkaddr(addr)
		checkaddr(system.address(type, expected.literal, expected.port))
		checkaddr(system.address(type, expected.literal, expected.port, "t"))
		checkaddr(system.address(type, expected.binary, expected.port, "b"))

		done()
	end

	do case("change "..type)
		for field, newval in pairs(expected.changes) do
			local oldval = addr[field]
			addr[field] = newval
			assert(addr[field] == newval)
			addr[field] = oldval
			checkaddr(addr)
		end

		for literal, binary in pairs(expected.equivalents) do
			addr.literal = literal
			assert(addr.binary == binary)
		end
		for literal, binary in pairs(expected.equivalents) do
			addr.binary = binary
			assert(addr.literal == literal)
		end

		done()
	end
end

do
	for _, type in ipairs{ "ipv4", "ipv6" } do case("bad field "..type)
		local a = system.address(type)
		asserterr("bad argument #2 to '__index' (invalid option 'wrongfield')",
			pcall(function () return a.wrongfield end))
		asserterr("bad argument #2 to '__newindex' (invalid option 'wrongfield')",
			pcall(function () a.wrongfield = true end))
		asserterr("bad argument #2 to '__newindex' (invalid option 'type')",
			pcall(function () a.type = true end))
		if type == "file" then
			asserterr("bad argument #2 to '__newindex' (invalid option 'port')",
				pcall(function () a.port = 1234 end))
		end

		done()
	end
end

do case "errors"
	asserterr("bad argument #1 to 'coutil.system.address' (invalid option 'ip')",
		pcall(system.address, "ip"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
		pcall(system.address, "ipv4", true))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
		pcall(system.address, "ipv4", true, 8080))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
		pcall(system.address, "ipv4", true, 8080, "t"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got boolean)",
		pcall(system.address, "ipv4", true, 8080, "b"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, nil))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, nil, nil))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, 8080))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, 8080, "t"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, 8080, "b"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, nil))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, nil, "t"))
	asserterr("bad argument #2 to 'coutil.system.address' (string or memory expected, got nil)",
		pcall(system.address, "ipv4", nil, nil, "b"))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
		pcall(system.address, "ipv4", "192.168.0.1:8080", nil))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
		pcall(system.address, "ipv4", "192.168.0.1", nil, "t"))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got nil)",
		pcall(system.address, "ipv4", "\192\168\0\1", nil, "b"))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
		pcall(system.address, "ipv4", "192.168.0.1", "port"))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
		pcall(system.address, "ipv4", "192.168.0.1", "port", "t"))
	asserterr("bad argument #3 to 'coutil.system.address' (number expected, got string)",
		pcall(system.address, "ipv4", "192.168.0.1", "port", "b"))
	asserterr("bad argument #4 to 'coutil.system.address' (invalid mode)",
		pcall(system.address, "ipv4", 3232235776, 8080, "n"))

	asserterr("bad argument #2 to 'coutil.system.address' (invalid URI format)",
		pcall(system.address, "ipv4", "192.168.0.1"))
	asserterr("invalid argument",
		pcall(system.address, "ipv4", "localhost:8080"))
	asserterr("invalid argument",
		pcall(system.address, "ipv4", "291.168.0.1:8080"))
	asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "192.168.0.1:65536"))
	asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "192.168.0.1:-8080"))
	asserterr("bad argument #2 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "192.168.0.1:0x1f90"))

	asserterr("invalid argument",
		pcall(system.address, "ipv4", "localhost", 8080, "t"))
	asserterr("invalid argument",
		pcall(system.address, "ipv4", "291.168.0.1", 8080, "t"))

	asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "192.168.0.1", 65536, "t"))
	asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "192.168.0.1", -1, "t"))
	asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "\192\168\000\001", 65536, "b"))
	asserterr("bad argument #3 to 'coutil.system.address' (invalid port)",
		pcall(system.address, "ipv4", "\192\168\000\001", -1, "b"))

	done()
end
