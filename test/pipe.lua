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

do case "getaddress"
	local sock = assert(create("stream"))

	assert(sock:bind(pipenames.free) == true)
	asserterr("invalid argument", sock:bind(pipenames.bindable))

	local addr = sock:getaddress()
	assert(addr == pipenames.free)
	local addr = sock:getaddress("this")
	assert(addr == pipenames.free)
	asserterr("socket is not connected", sock:getaddress("peer"))

	local a = sock:getaddress(nil, addr)
	assert(rawequal(a, addr))
	assert(a == pipenames.free)
	local a = sock:getaddress("this", addr)
	assert(rawequal(a, addr))
	assert(a == pipenames.free)
	asserterr("socket is not connected", sock:getaddress("peer", addr))

	done()
end

teststream(create, pipenames)

os.remove(pipenames.bindable)
os.remove(pipenames.free)