local _G = require "_G"
local assert = _G.assert
local type = _G.type
local xpcall = _G.xpcall

local coroutine = require "coroutine"
local newthread = coroutine.create
local resume = coroutine.resume


local function onterm(handler, success, ...)
	if success then
		return handler(true, ...)
	end
end

local function trapcall(f, handler, ...)
	local function onerror(...)
		return handler(false, ...)
	end
	onterm(handler, xpcall(f, onerror, ...))
end

local module = {}

function module.trap(handler, f, ...)
	local thread = newthread(trapcall)
	resume(thread, f, handler, ...)
	return thread
end

function module.catch(handler, f, ...)
	local thread = newthread(xpcall)
	resume(thread, f, handler, ...)
	return thread
end

return module
