local _G = require "_G"
local assert = _G.assert
local type = _G.type
local xpcall = _G.xpcall

local coroutine = require "coroutine"
local newthread = coroutine.create
local resume = coroutine.resume


local function onterm(handler, success, ...)
	if success then
		handler(true, ...)
	end
end

local function trapcall(f, handler, ...)
	local function onerror(...)
		handler(false, ...)
	end
	onterm(handler, xpcall(f, onerror, ...))
end

local module = {}

function module.trap(handler, f, ...)
	assert(type(handler) == "function", "bad argument #1 (function expected)")
	resume(newthread(trapcall), f, handler, ...)
end

function module.catch(handler, f, ...)
	assert(type(handler) == "function", "bad argument #1 (function expected)")
	resume(newthread(xpcall), f, handler, ...)
end

return module
