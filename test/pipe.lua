local memory = require "memory"
local system = require "coutil.system"

local pipenames = {
	free = "./freeaddress.usock",
	bindable = "./localaddress.usock",
	denied = "/dev/null",
}
if standard == "win32" then
	pipenames.free = [[\\?\pipe\freeaddress.pipe]]
	pipenames.bindable = [[\\?\pipe\localaddress.pipe]]
	pipenames.denied = nil
end

local function testgetdomain(create, domain, ...)
	case "getdomain"

	local sock = assert(create(...))
	assert(sock:getdomain() == domain)

	done()
end

local function testgetaddr(create)
	case "getaddress"

	local sock = assert(create("stream"))

	assert(sock:bind(pipenames.free) == true)
	asserterr("invalid argument", sock:bind(pipenames.bindable))

	local addr = sock:getaddress()
	assert(addr == pipenames.free, tostring(addr))
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

newgroup("pipe") ---------------------------------------------------------------

local function create(kind) return system.socket(kind, "local") end

newtest "creation"

testobject(create, "passive")
testobject(create, "stream")

newtest "address"

testgetdomain(create, "local", "passive")
testgetdomain(create, "local", "stream")
testgetaddr(create)
teststream(create, pipenames)

gc()
for name, path in pairs(pipenames) do
	os.remove(path)
end

newgroup("share") ----------------------------------------------------------------

local function create(kind) return system.socket(kind, "share") end

newtest "creation"

testobject(create, "passive")
testobject(create, "stream")

newtest "address"

testgetdomain(create, "share", "passive")
testgetdomain(create, "share", "stream")
testgetaddr(create)
teststream(create, pipenames)

if standard == "posix" then case "tranfer socket"
	local serveraddr = os.tmpname()
	local parentaddr = os.tmpname()
	os.remove(parentaddr)
	os.remove(serveraddr)
	if standard == "win32" then
		serveraddr = [[\\?\pipe\]]..serveraddr:match("[^\\/]+$")
		parentaddr = [[\\?\pipe\]]..parentaddr:match("[^\\/]+$")
	end
	
	local done1
	spawn(function ()
		local parent<close> = assert(system.socket("passive", "share"))
		assert(parent:bind(parentaddr))
		assert(parent:listen(1))

		local server<close> = assert(system.socket("passive", "local"))
		assert(server:bind(serveraddr))
		assert(server:listen(1))

		local child<close> = assert(parent:accept())
		local client<close> = assert(server:accept())
		assert(child:write("parent", 1, -1, client))
		done1 = true
	end)
	assert(done1 == nil)

	local done2
	spawn(function ()
		local stream<close> = assert(system.socket("stream", "local"))
		assert(stream:connect(serveraddr))
		assert(stream:write("client"))

		local buffer = memory.create(#("child"))
		local bytes = 0
		repeat
			bytes = bytes+assert(stream:read(buffer, bytes+1))
		until bytes == #buffer
		assert(not memory.diff(buffer, "child"))
		done2 = true
	end)
	assert(done2 == nil)

	local done3
	spawn(function ()
		local childspec = {
			execfile = luabin,
			arguments = { "-" },
			stdin = "w",
		}
		spawn(function ()
			local res, value = system.execute(childspec)
			assert(res == "exit")
			assert(value == 0)
			done3 = true
		end)
		assert(childspec.stdin:write(string.format([[%s
			local memory = require "memory"
			local system = require "coutil.system"
			local done3
			spawn(function ()
				local parent<close> = assert(system.socket("stream", "share"))
				assert(parent:connect(%q))

				local buffer = memory.create(#("parent"))
				local bytes, client<close> = assert(parent:read(buffer))
				assert(bytes == #("parent"))
				assert(not memory.diff(buffer, "parent"))

				assert(client:write("child"))
				done3 = true
			end)
			assert(done3 == nil)
			system.run()
			assert(done3 == true)
		]], utilschunk, parentaddr)))
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