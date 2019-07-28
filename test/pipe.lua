local system = require "coutil.system"

local pipenames = {
	free = "./freeaddress.usock",
	bindable = "./localaddress.usock",
	denied = "/dev/null",
}

newgroup("pipe") ---------------------------------------------------------------

newtest "creation"

do case "error messages"
	for value in pairs(types) do
		asserterr("bad argument", pcall(system.pipe, value))
	end

	done()
end

local function create(kind) return system.pipe(kind) end

testobject(system.pipe, "passive")
testobject(system.pipe, "active")

newtest "address"

do case "getaddress"
	local sock = assert(system.pipe("active"))

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

teststream(system.pipe, pipenames)

os.remove(pipenames.bindable)
os.remove(pipenames.free)