package="coutil"
version="scm-1"
source = {
	url = "git://github.com/renatomaia/coutil",
}
description = {
	summary = "Coroutine utilities to support multithread in Lua",
	detailed = [[
		Lua libraries that provide support for:
		- Coroutine execution finalizers.
		- Coroutine syncronization abstractions (events, mutexes, and promises)
		- Coroutine resumption on conditions of system features (and API to such features):
			- Measure of time lapse;
			- Signals of system processes;
			- Program execution;
			- Data transmission over network and IPC sockets;
			- DNS lookups;
			- Code chunk execution on separate system threads:
				- Preemptive coroutines;
				- Thread pools;
				- Copy values between code chunks.
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
		header = "lmemlib.h",
		library = "luamem",
	},
	LIBUV = {
		header = "uv.h",
		library = "uv",
	},
}
build = {
	type = "make",
	build_variables = {
		SYSCFLAGS="$(CFLAGS)",
		SYSLIBFLAG="$(LIBFLAG)",
		LUA_LIB="$(LUALIB)",
		LUA_LIBDIR="$(LUA_LIBDIR)",
		LUA_BINDIR="$(LUA_BINDIR)",
		LUA_INCDIR="$(LUA_INCDIR)",
		LUAMEM_LIBDIR="$(LUAMEM_LIBDIR)",
		LUAMEM_INCDIR="$(LUAMEM_INCDIR)",
		LIBUV_LIBDIR="$(LIBUV_LIBDIR)",
		LIBUV_INCDIR="$(LIBUV_INCDIR)",
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
			["coutil"] = "coutil.dll",
		},
	},
	platforms = {
		windows = { makefile = "etc/Makefile.win" },
	},
}