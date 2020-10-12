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
	"lua >= 5.4, < 5.5",
	"vararg >= 2.1, < 3.0",
}
external_dependencies = {
	LUAMEMORY = {
		header = "lmemlib.h",
		library = "lmemlib",
	},
	LIBUV = {
		header = "uv.h",
		library = "uv",
	},
}
build = {
	type = "builtin",
	modules = {
		["coutil.mutex"] = "lua/coutil/mutex.lua",
		["coutil.queued"] = "lua/coutil/queued.lua",
		["coutil.event"] = "lua/coutil/event.lua",
		["coutil.promise"] = "lua/coutil/promise.lua",
		["coutil.spawn"] = "lua/coutil/spawn.lua",
		["coutil"] = {
			sources = {
				"src/lmodaux.c", "src/loperaux.c", "src/lchaux.c", "src/lthpool.c",
				"src/lscheduf.c", "src/ltimef.c", "src/lprocesf.c", "src/lsocketf.c",
				"src/lcoroutm.c", "src/lthreadm.c", "src/lchannem.c", "src/lsystemm.c",
			},
			libdirs = { "$(LUAMEMORY_LIBDIR)", "$(LIBUV_LIBDIR)" },
			incdirs = { "$(LUAMEMORY_INCDIR)", "$(LIBUV_INCDIR)" },
			libraries = {"lmemlib", "uv"},
		},
	}
}
