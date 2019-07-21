local spawn = require "coutil.spawn"
local catch = spawn.catch
local trap = spawn.trap

newtest "catch" --------------------------------------------------------------

do case "error messages"
	assert(coroutine.status(catch()) == "dead")
	assert(coroutine.status(catch(nil)) == "dead")
	assert(coroutine.status(catch(nil, nil)) == "dead")

	local c = counter()
	assert(coroutine.status(catch(c, nil)) == "dead")
	assert(c() == 2)
	assert(coroutine.status(catch(nil, c)) == "dead")
	assert(c() == 3)

	done()
end

do case "spawn thread"
	local c = counter()

	local thread
	catch(c, function ()
		thread = coroutine.running()
	end)
	assert(c() == 1)
	assert(coroutine.status(thread) == "dead")

	local failed, catcher
	local function catcherr(msg)
		catcher = coroutine.running()
	end
	catch(catcherr, function ()
		failed = coroutine.running()
		error("oops!")
	end)
	assert(coroutine.status(failed) == "dead")
	assert(catcher == failed)
	assert(thread ~= failed)

	done()
end

do case "garbage collection"
	local other = coroutine.running()

	for _, fail in ipairs{true, false} do
		local function func(fail)
			garbage[fail] = coroutine.running()
			if fail then error("oops!") end
			coroutine.yield()
		end
		catch(function () end, func, fail)
		assert(garbage[fail] ~= nil)
	end

	done()
end

do case "parameter values"
	local values
	local function packargs(...)
		values = table.pack(...)
	end
	catch(function () end, packargs, table.unpack(types))
	for index, value in ipairs(types) do
		assert(values[index] == value)
	end

	done()
end

do case "error values"
	for _, e in ipairs(types) do
		local err, narg
		local function catcherr(msg, ...)
			err = msg
			narg = select("#", ...)
		end
		catch(catcherr, error, e)
		assert(err == e)
		assert(narg == 0)
	end

	done()
end

do case "resume failure"
	for _, e in ipairs(types) do
		if type(e) ~= "string" then
			local thread
			local function func()
				thread = coroutine.running()
				coroutine.yield()
				error(e)
			end
			local err, narg
			local function catcherr(msg, ...)
				err = msg
				narg = select("#", ...)
			end
			catch(catcherr, func, e)
			assert(coroutine.status(thread) == "suspended")
			assert(err == nil)
			assert(narg == nil)
			assert(coroutine.resume(thread))
			assert(coroutine.status(thread) == "dead")
			assert(err == e)
			assert(narg == 0)
		end
	end

	done()
end

do case "handler error"
	local thread
	local function func()
		thread = coroutine.running()
		coroutine.yield()
		error("oops!")
	end
	local function raiseerr()
		error("oops!")
	end
	catch(raiseerr, func)
	assert(coroutine.status(thread) == "suspended")
	local resumeok, xpcallok, xpcallerr = coroutine.resume(thread)
	assert(coroutine.status(thread) == "dead")
	assert(resumeok == true)
	assert(xpcallok == false)
	assert(xpcallerr == "error in error handling")

	done()
end

newtest "trap" --------------------------------------------------------------

do case "error messages"
	assert(coroutine.status(trap()) == "dead")
	assert(coroutine.status(trap(nil)) == "dead")
	assert(coroutine.status(trap(nil, nil)) == "dead")

	local c = counter()
	assert(coroutine.status(trap(c, nil)) == "dead")
	assert(c() == 2)
	assert(coroutine.status(trap(nil, c)) == "dead")
	assert(c() == 4)

	done()
end

do case "spawn thread"
	local other = coroutine.running()

	for _, fail in ipairs{true, false} do
		local trapper
		local function handler()
			trapper = coroutine.running()
		end
		local thread
		local function func(fail)
			thread = coroutine.running()
			if fail then error("oops!") end
		end
		trap(handler, func, fail)
		assert(coroutine.status(thread) == "dead")
		assert(thread == trapper)
		assert(thread ~= other)
		other = thread
	end

	done()
end

do case "garbage collection"
	local other = coroutine.running()

	for _, fail in ipairs{true, false} do
		local function func(fail)
			garbage[fail] = coroutine.running()
			if fail then error("oops!") end
			coroutine.yield()
		end
		trap(function () end, func, fail)
		assert(garbage[fail] ~= nil)
	end

	done()
end

do case "parameter values"
	local values
	local function packargs(...)
		values = table.pack(...)
	end
	trap(function () end, packargs, table.unpack(types))
	for index, value in ipairs(types) do
		assert(values[index] == value)
	end

	done()
end

do case "result values"
	for _, v in ipairs(types) do
		for _, fail in ipairs{true, false} do
			local values
			local function packargs(...)
				values = table.pack(...)
			end
			local function func(fail)
				if fail then error(v) end
				return v, v, v
			end
			trap(packargs, func, fail)
			if fail then
				assert(values[1] == false)
				assert(type(v) == "string" or values[2] == v)
				assert(values.n == 2)
			else
				assert(values[1] == true)
				assert(values[2] == v)
				assert(values[3] == v)
				assert(values[4] == v)
				assert(values.n == 4)
			end
		end
	end

	done()
end

do case "resumed results"
	for _, v in ipairs(types) do
		for _, fail in ipairs{true, false} do
			local values
			local function packargs(...)
				values = table.pack(...)
			end
			local thread
			local function func(fail)
				thread = coroutine.running()
				coroutine.yield()
				if fail then error(v) end
				return v, v, v
			end
			trap(packargs, func, fail)
			assert(coroutine.status(thread) == "suspended")
			assert(values == nil)
			assert(coroutine.resume(thread))
			assert(coroutine.status(thread) == "dead")
			if fail then
				assert(values[1] == false)
				assert(type(v) == "string" or values[2] == v)
				assert(values.n == 2)
			else
				assert(values[1] == true)
				assert(values[2] == v)
				assert(values[3] == v)
				assert(values[4] == v)
				assert(values.n == 4)
			end
		end
	end

	done()
end

do case "handler error"
	for _, fail in ipairs{true, false} do
		local thread
		local function func(fail)
			thread = coroutine.running()
			coroutine.yield()
			if fail then error("oops!") end
		end
		local function raiseerr()
			error("oops!")
		end
		trap(raiseerr, func, fail)
		assert(coroutine.status(thread) == "suspended")
		local succ, res = coroutine.resume(thread)
		assert(coroutine.status(thread) == "dead")
		if fail then
			assert(succ == true)
			assert(res == nil)
		else
			asserterr("oops!", succ, res)
		end
	end

	done()
end
