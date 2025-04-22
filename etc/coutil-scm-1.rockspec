local luamem_release = "2.1.0-1"

package = "coutil"
version = "scm-1"
source = {
	url = "git://github.com/renatomaia/coutil",
}
description = {
	summary = "Coroutine utilities to support multithread in Lua",
	detailed = [[
		coutil provides a set of integrated libraries to support multithreading in Lua;
		both cooperatively using coroutines and preemptively by running code on distinct
		system threads. It also provides synchronous (no callbacks) and non-blocking
		(suspends only the caller) functions for a variety of features: from synchronization
		mechanisms like events, channels, and others; to access to system resources like
		networking, processes, file system, and more.
	]],
	homepage = "https://github.com/renatomaia/coutil",
	license = "MIT/X11"
}
dependencies = {
	"lua >= 5.4, < 5.5",
	"vararg >= 2.1, < 3.0",
	"memory == "..luamem_release,
}
external_dependencies = {
	LIBUV = {
		header = "uv.h",
		library = "uv",
	},
}
build = {
	type = "cmake",
	variables = {
		CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS = "ON",
		CMAKE_INSTALL_PREFIX = "$(PREFIX)",
		CMAKE_LIBRARY_PATH = "$(LUA_LIBDIR);$(ROCKS_TREE)/memory/"..luamem_release.."/library;$(LIBUV_LIBDIR)",
		LUA_INCLUDE_DIR = "$(LUA_INCDIR)",
		LUAMEM_INCLUDE_DIR = "$(ROCKS_TREE)/memory/"..luamem_release.."/include",
		LIBUV_INCLUDE_DIR = "$(LIBUV_INCDIR)",
		MODULE_DESTINATION = "$(LIBDIR)",
	},
	install = {
		bin = { coutil = "demo/console.lua" },
		lua = {
			["coutil.event"] = "lua/coutil/event.lua",
			["coutil.mutex"] = "lua/coutil/mutex.lua",
			["coutil.promise"] = "lua/coutil/promise.lua",
			["coutil.queued"] = "lua/coutil/queued.lua",
			["coutil.spawn"] = "lua/coutil/spawn.lua",
		},
	},
	copy_directories = {
		"demo",
		"doc",
		"test",
	},
}
