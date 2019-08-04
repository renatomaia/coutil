local _G = require "_G"
local error = _G.error
local select = _G.select
local setmetatable = _G.setmetatable

local coroutine = require "coroutine"
local isyieldable = coroutine.isyieldable
local resume = coroutine.resume
local running = coroutine.running
local yield = coroutine.yield

local vararg = require "vararg"
local countargs = vararg.len
local vaconcat = vararg.concat
local varange = vararg.range

local listof = setmetatable({}, { __mode = "k" })

local function addthread(event, thread)
	local list = listof[event]
	if list == nil then
		list = { [thread] = thread, tail = thread }
		listof[event] = list
		return list
	elseif list[thread] == nil then
		local tail = list.tail
		list[thread] = list[tail]
		list[tail] = thread
		list.tail = thread
		return list
	end
end

local function removethread(event, thread, list)
	local following = list[thread]
	if following == thread then
		list[thread] = nil
		list.tail = nil
		if listof[event] == list then
			listof[event] = nil
		end
		return thread
	elseif following ~= nil then
		local tail = list.tail
		local previous
		local current = tail
		repeat
			previous, current = current, list[current]
		until current == thread
		if tail == thread then
			list.tail = previous
		end
		list[previous] = following
		list[thread] = nil
		return thread
	end
end

local function yieldablecaller()
	if not isyieldable() then
		error("unable to yield", 3)
	end
	return running()
end

local module = { version = "1.0 alpha" }

function module.pending(event)
	return listof[event] ~= nil
end

function module.emitall(event, ...)
	local list = listof[event]
	if list ~= nil then
		listof[event] = nil
		repeat
			local tail = list.tail
			local head = list[tail]
			if head == tail then
				list.tail = nil
			else
				list[tail] = list[head]
			end
			list[head] = nil
			resume(head, listof, event, ...)
		until list.tail == nil
		return true
	end
	return false
end

function module.emitone(event, ...)
	local list = listof[event]
	if list ~= nil then
		local tail = list.tail
		local head = list[tail]
		if head == tail then
			list.tail = nil
			listof[event] = nil
		else
			list[tail] = list[head]
		end
		list[head] = nil
		resume(head, listof, event, ...)
		return true
	end
	return false
end

do
	local function cancel(event, thread, list, ...)
		if ... == listof then
			return select(2, ...)
		end
		removethread(event, thread, list)
		return ...
	end

	function module.await(event)
		local thread = yieldablecaller()
		local list = addthread(event, thread)
		return cancel(event, thread, list, yield())
	end
end

do
	local function cancel(thread, regsz, ...)
		for index = 1, regsz, 2 do
			local event, list = select(index, ...)
			removethread(event, thread, list)
		end
		return select(regsz+1, ...)
	end

	local function register(thread, count, event, ...)
		if count > 0 then
			if event ~= nil then
				local list = addthread(event, thread)
				if list ~= nil then
					return event, list, register(thread, count-1, ...)
				end
			end
			return register(thread, count-1, ...)
		end
	end

	local function resumed(thread, regsz, count, ...)
		if select(regsz+1, ...) ~= listof then
			return cancel(thread, regsz, ...)
		elseif count == 0 then
			return true
		end
		return resumed(thread, regsz, count-1,
		               vaconcat(yield, varange(1, regsz, ...)))
	end

	local function registered(thread, ...)
		local regsz = countargs(...)
		if regsz == 0 then
			return true
		end
		return resumed(thread, regsz, regsz//2-1, vaconcat(yield, ...))
	end

	function module.awaitall(...)
		local thread = yieldablecaller()
		return registered(thread, register(thread, countargs(...), ...))
	end

	local function resumed(thread, ...)
		if ... == listof then
			return select(2, ...)
		end
		return ...
	end

	local function registered(thread, ...)
		local regsz = countargs(...)
		if regsz <= 0 then
			error("non-nil value expected", 3)
		end
		return resumed(thread, cancel(thread, regsz, vaconcat(yield, ...)))
	end

	function module.awaitany(...)
		local thread = yieldablecaller()
		return registered(thread, register(thread, countargs(...), ...))
	end
end

return module
