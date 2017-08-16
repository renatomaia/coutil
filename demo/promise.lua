local promise = require "coutil.promise"

do
	local fulfillmentOf = {}

	function asyncop()
		local r, f = promise.create()
		fulfillmentOf[r] = f
		return r
	end

	local count = 0

	function endasyncop(r)
		local f
		if r == nil then
			r, f = next(fulfillmentOf)
			if r == nil then return false end
		else
			f = assert(fulfillmentOf[r], "invalid request")
		end
		fulfillmentOf[r] = nil
		count = count+1
		f(true, count)
		return true
	end
end

-------------------------------------------

co = coroutine.create(function (r)
	local r = asyncop()
	print("immediate result:", r("probe"))
	print("after await:", r())
end)
assert(coroutine.resume(co, r)) -- will yield to wait completion
endasyncop() -- will resume 'co' and it will finish

co = coroutine.create(function (r)
	local r1 = asyncop()
	local r2 = asyncop()
	local r3 = asyncop()

	local r = promise.pickready(r1, r2, r3)
	if r ~= nil then
		print("one is ready:", r())
	else
		print("none is ready yet ...")
		print("after await:", promise.awaitany(r1, r2, r3)())
	end
end)
assert(coroutine.resume(co, r)) -- will yield to wait completion
endasyncop() -- will resume 'co' and it will finish

co = coroutine.create(function (r)
	local r1 = asyncop()
	local r2 = asyncop()
	local r3 = asyncop()

	promise.awaitall(r1, r2, r3)

	print("r1:", r1())
	print("r2:", r2())
	print("r3:", r3())
end)
assert(coroutine.resume(co, r)) -- will yield to wait completion
repeat until not endasyncop() -- will resume 'co' until there is no pending calls
