package="coutil"
version="scm-1"
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
	"lua >= 5.4",
	"vararg >= 2.1",
}
external_dependencies = {
	LUAMEM = {
		header = "luamem.h",
		library = "luamem",
	},
	LIBUV = {
		header = "uv.h",
		library = "uv",
	},
}
build = {
	type = "make",
	makefile = "src/Makefile",
	build_target = "fromrockspec",
	build_variables = {
		SYSCFLAGS = "$(CFLAGS)",
		SYSLIBFLAG = "$(LIBFLAG)",
		LUA_LIB = "$(LUALIB)",
		LUA_LIBDIR = "$(LUA_LIBDIR)",
		LUA_INCDIR = "$(LUA_INCDIR)",
		LUAMEM_LIBDIR = "$(LUAMEM_LIBDIR)",
		LUAMEM_INCDIR = "$(LUAMEM_INCDIR)",
		LIBUV_LIBDIR = "$(LIBUV_LIBDIR)",
		LIBUV_INCDIR = "$(LIBUV_INCDIR)",
	},
	install_pass = false,
	install = {
		lua = {
			["coutil.event"] = "lua/coutil/event.lua",
			["coutil.mutex"] = "lua/coutil/mutex.lua",
			["coutil.promise"] = "lua/coutil/promise.lua",
			["coutil.queued"] = "lua/coutil/queued.lua",
			["coutil.spawn"] = "lua/coutil/spawn.lua",
		},
		lib = {
			["coutil"] = "src/coutil.so",
		},
	},
	platforms = {
		windows = {
			makefile = "etc/Makefile.win",
			build_target = "mod",
			install = {
				lib = {
					["coutil"] = "coutil.dll",
				},
			},
		},
	},
}