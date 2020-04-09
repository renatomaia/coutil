local system = require "coutil.system"

newtest "coroutine" ------------------------------------------------------------

do case "error messages"
	for v in ipairs(types) do
		if type(v) ~= "string" then
			asserterr("string or memory expected", pcall(system.coroutine, v))
			asserterr("string expected", pcall(system.coroutine, "", v))
			asserterr("string expected", pcall(system.coroutine, "", "", v))
		end
	end
	asserterr("syntax error", system.coroutine("invalid chunk"))

	done()
end

do case "runtime errors"
	local co = system.coroutine[[ error "Oops!" ]]
	asserterr("Oops!", system.resume(co))

	done()
end

do case "parallel execution"
	local path = os.tmpname()
	os.remove(path)

	assert(io.open(path) == nil)
	local co = assert(system.coroutine(string.format([[
		local io = require "io"
		local os = require "os"
		local path = %q
		while true do
			local file = io.open(path)
			if file ~= nil then
				local contents = file:read("a")
				file:close()
				if contents  == "deleteme" then
					os.remove(path)
					break
				end
			end
		end
	]], path)) == true)

	local stage = 0
	spawn(function ()
		assert(system.resume(co))
		stage = 1
	end)

	local file = assert(io.open(path, "w"))
	file:write("deleteme")
	file:close()

	local removed = false
	for i=1, 1e3 do
		if io.open(path) == nil then
			removed = true
			break
		end
	end

	assert(removed)

	system.run()

	assert(stage == 1)

	done()
end

do return end


local function startsynced(chunk, ...)
	local synced = { path = os.tmpname() }
	os.remove(synced.path)

	assert(io.open(synced.path) == nil)
	local co = assert(system.coroutine(string.format([[%s
		local io = require "io"
		local os = require "os"
		local path = %q
		local file = io.open(path, "w")
		file:write("OK")
		file:close()
	]], chunk, synced.path), ...) == true)
	spawn(function ()
		synced.results = table.pack(system.resume(co))
	end)

	return synced
end

local function waitsynced(path)
	local result
	for i=1, 1e3 do
		result = readfrom(path)
		if result ~= nil then
			break
		end
	end
	assert(result == "OK")
end

do case "arguments"
	waitsynced(startsynced("assert(... == nil)"))
	waitsynced(startsynced("assert(... == nil)", nil))
	waitsynced(startsynced("assert(... == false)", false))
	waitsynced(startsynced("assert(... == true)", true))
	waitsynced(startsynced("assert(... == 0)", 0))
	waitsynced(startsynced("assert(... == 1)", 1))
	waitsynced(startsynced("assert(... == -1)", -1))
	waitsynced(startsynced("assert(... == 0xadap-16)", 0xadap-16))
	waitsynced(startsynced("assert(... == '')", ""))
	waitsynced(startsynced("assert(... == 'Lua 5.4')", "Lua 5.4"))
	waitsynced(startsynced("assert(... == '\\0')", "\0"))
	waitsynced(startsynced([[
		local a,b,c,d,e = ...
		assert(a == nil)
		assert(b == false)
		assert(c == 123)
		assert(d == 0xfacep-8)
		assert(e == "\001")
	]]), nil, false, 123, 0xfacep-8, "\001")

	done()
end

do case "yield"
	local synced = startsynced[[
		for i = 1, 10 do
			coroutine.yield()
		end
	]]

	waitsynced(synced)

	done()
end

do case "resume suspended"

	done()
end

do case "resume running"

	done()
end

do case "resume closed"

	done()
end

do case "resume dead"

	done()
end

do case "running unreferenced"

	done()
end

do case "collect suspended"
	local stage
	spawn(function ()
		garbage.co = system.coroutine("coroutine.yield('Hello')")
		stage = 0
		local ok, res = system.resume(garbage.co)
		assert(ok == true)
		assert(res == "Hello")
		gc()
		assert(garbage.co == nil)
		stage = 1
	end)
	assert(stage == 0)

	assert(system.run() == true)
	assert(stage == 1)

	done()
end

do case "collect cancelled"

	done()
end

do case "collect running"
	local stage
	garbage.co = system.coroutine("coroutine.yield('Hello')")

	spawn(function ()
		stage = 0
		local ok, res = system.resume(garbage.co)
		assert(ok == false)
		assert(res == "closed")
		stage = 1
	end)
	assert(stage == 0)

	gc()
	assert(garbage.co == nil)
	assert(stage == 1)

	assert(system.run() == false)

	done()
end

do case "close running"
	local stage
	local co = system.coroutine("coroutine.yield('Hello')")

	spawn(function ()
		stage = 0
		local ok, res = system.resume(co)
		assert(ok == false)
		assert(res == "closed")
		stage = 1
	end)
	assert(stage == 0)

	co:close()
	assert(stage == 1)

	assert(system.run() == false)

	done()
end
