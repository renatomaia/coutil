local memory = require "memory"
local system = require "coutil.system"

local pipenames = {
	free = "./freeaddress.usock",
	bindable = "./localaddress.usock",
	denied = "/dev/null",
}

newgroup("pipe") ---------------------------------------------------------------

local function create(kind) return system.socket(kind, "local") end

newtest "creation"

testobject(create, "passive")
testobject(create, "stream")

newtest "address"

local function testgetaddr(create)
	case "getaddress"

	local sock = assert(create("stream"))

	assert(sock:bind(pipenames.free) == true)
	asserterr("invalid argument", sock:bind(pipenames.bindable))

	local addr = sock:getaddress()
	assert(addr == pipenames.free)
	local addr = sock:getaddress("self")
	assert(addr == pipenames.free)
	asserterr("socket is not connected", sock:getaddress("peer"))

	local a = sock:getaddress(nil, addr)
	assert(rawequal(a, addr))
	assert(a == pipenames.free)
	local a = sock:getaddress("self", addr)
	assert(rawequal(a, addr))
	assert(a == pipenames.free)
	asserterr("socket is not connected", sock:getaddress("peer", addr))

	done()
end

testgetaddr(create)
teststream(create, pipenames)

gc()
for name, path in pairs(pipenames) do
	os.remove(path)
end

newgroup("ipc") ----------------------------------------------------------------

local function create(kind) return system.socket(kind, "ipc") end

newtest "creation"

testobject(create, "passive")
testobject(create, "stream")

newtest "address"

testgetaddr(create)
teststream(create, pipenames)

do case "tranfer socket"
	local serveraddr = os.tmpname()
	local parentaddr = os.tmpname()
	os.remove(parentaddr)
	os.remove(serveraddr)

	local done1
	spawn(function ()
		local parent<close> = assert(system.socket("passive", "ipc"))
		assert(parent:bind(parentaddr))
		assert(parent:listen(1))

		local server<close> = assert(system.socket("passive", "local"))
		assert(server:bind(serveraddr))
		assert(server:listen(1))

		local child<close> = assert(parent:accept())
		local client<close> = assert(server:accept())
		assert(child:send("parent", 1, -1, client))
		done1 = true
	end)
	assert(done1 == nil)

	local done2
	spawn(function ()
		local stream<close> = assert(system.socket("stream", "local"))
		assert(stream:connect(serveraddr))
		assert(stream:send("client"))

		local buffer = memory.create(#("child"))
		local bytes = 0
		repeat
			bytes = bytes+assert(stream:receive(buffer, bytes+1))
		until bytes == #buffer
		assert(not memory.diff(buffer, "child"))
		done2 = true
	end)
	assert(done2 == nil)

	local done3
	spawn(function ()
		local childspec = {
			execfile = luabin,
			arguments = { "--" },
			stdin = "w",
		}
		spawn(function ()
			local res, value = system.execute(childspec)
			assert(res == "exit")
			assert(value == 0)
			done3 = true
		end)
		assert(childspec.stdin:send(utilschunk..[[
			local memory = require "memory"
			local system = require "coutil.system"
			local done3
			spawn(function ()
				local parent<close> = assert(system.socket("stream", "ipc"))
				assert(parent:connect("]]..parentaddr..[["))

				local buffer = memory.create(#("parent"))
				local bytes, client<close> = assert(parent:receive(buffer))
				assert(bytes == #("parent"))
				assert(not memory.diff(buffer, "parent"))

				assert(client:send("child"))
				done3 = true
			end)
			assert(done3 == nil)
			system.run()
			assert(done3 == true)
		]]))
		assert(childspec.stdin:shutdown())
	end)
	assert(done3 == nil)

	gc()
	assert(system.run() == false)
	assert(done1 == true)
	assert(done2 == true)
	assert(done3 == true)

	done()
end

os.remove(pipenames.bindable)
os.remove(pipenames.free)