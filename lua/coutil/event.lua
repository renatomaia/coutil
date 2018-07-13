local _G = require "_G"
local assert = _G.assert
local error = _G.error
local next = _G.next
local pairs = _G.pairs
local pcall = _G.pcall
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
			resume(head, event, ...)
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
		resume(head, event, ...)
		return true
	end
	return false
end

function module.await(event)
	assert(isyieldable(), "unable to yield")
	addthread(event, running())
	return select(2, yield())
end

function module.awaitall(...)
	assert(isyieldable(), "unable to yield")
	local thread = running()
	local count = 0
	for index = 1, countargs(...) do
		local event = select(index, ...)
		if event ~= nil and addthread(event, thread) ~= nil then
			count = count+1
		end
	end
	for _ = 1, count do
		yield()
	end
end

do
	local function cancel(thread, count, ...)
		for index = 1, count, 2 do
			local event, list = select(index, ...)
			removethread(event, thread, list)
		end
		return select(count+1, ...)
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

	local function await(thread, ...)
		local count = countargs(...)
		assert(count > 0, "non-nil value expected")
		return cancel(thread, count, vaconcat(yield, ...))
	end

	function module.awaitany(...)
		assert(isyieldable(), "unable to yield")
		local thread = running()
		return await(thread, register(thread, countargs(...), ...))
	end
end

do
	local function cancel(thread, events)
		for event, list in pairs(events) do
			removethread(event, thread, list)
		end
	end

	local function notify(callback, events, event, ...)
		events[event] = nil
		return pcall(callback, event, ...)
	end

	local function handle(callback, thread, events, success, ...)
		if not success then
			cancel(thread, events)
			error(...)
		elseif countargs(...) > 0 then
			cancel(thread, events)
			return ...
		elseif next(events) ~= nil then
			return handle(callback, thread, events, notify(callback, events, yield()))
		end
	end

	function module.awaiteach(callback, ...)
		assert(isyieldable(), "unable to yield")
		local thread = running()
		local events = {}
		for index = 1, countargs(...) do
			local event = select(index, ...)
			if event ~= nil and events[event] == nil then
				events[event] = addthread(event, thread)
			end
		end
		if next(events) ~= nil then
			return handle(callback, thread, events, notify(callback, events, yield()))
		end
	end
end

return module
