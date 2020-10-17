#include "lmodaux.h"
#include "loperaux.h"

#include <string.h>
#include <signal.h>


static const struct { const char *name; int value; } signals[] = {
	{ "abort", SIGABRT },
	{ "continue", SIGCONT },
	{ "hangup", SIGHUP },
	{ "interrupt", SIGINT },
	{ "quit", SIGQUIT },
	{ "stop", SIGTSTP },
	{ "terminate", SIGTERM },
	{ "loosepipe", SIGPIPE },
	{ "bgread", SIGTTIN },
	{ "bgwrite", SIGTTOU },
	{ "cpulimit", SIGXCPU },
	{ "filelimit", SIGXFSZ },
	{ "child", SIGCHLD },
	{ "clocktime", SIGALRM },
	{ "debug", SIGTRAP },
	{ "urgsock", SIGURG },
	{ "user1", SIGUSR1 },
	{ "user2", SIGUSR2 },
#ifdef SIGPROF
	{ "cputimall", SIGPROF },
#endif
#ifdef SIGVTALRM
	{ "cputimprc", SIGVTALRM },
#endif
#ifdef SIGPOLL
	{ "polling", SIGPOLL },
#endif
#ifdef SIGSYS
	{ "sysargerr", SIGSYS },
#endif
#ifdef SIGWINCH
	{ "winresize", SIGWINCH },
#endif
	{ NULL, 0 },
};

static void pushsignal (lua_State *L, int signum) {
	int i;
	for (i=0; signals[i].name && (signals[i].value != signum); i++);
	if (signals[i].name) lua_pushstring(L, signals[i].name);
	else lua_pushinteger(L, signum);
	lua_pushinteger(L, signum);
}

static int checksignal (lua_State *L, int arg) {
	const char *name = luaL_checkstring(L, arg);
	int i;
	for (i=0; signals[i].name; i++)
		if (strcmp(signals[i].name, name) == 0)
			return signals[i].value;
	return luaL_argerror(L, arg,
	                     lua_pushfstring(L, "invalid signal '%s'", name));
}


/* success [, errmsg] = system.getpid ([which]) */
static int system_getpid (lua_State *L) {
	static const char *const options[] = {"self", "parent", NULL};
	int parent = luaL_checkoption(L, 1, "self", options);
	lua_pushinteger(L, (lua_Integer)(parent ? uv_os_getppid() : uv_os_getpid()));
	return 1;
}


/* success [, errmsg] = system.emitsig (process, signal) */
static int system_emitsig (lua_State *L) {
	int err = uv_kill(luaL_checkinteger(L, 1), checksignal(L, 2));
	return lcuL_pushresults(L, 0, err);
}


/* signal [, errmsg] = system.awaitsig(signal) */
static int returnsignal (lua_State *L) {
	if (checksignal(L, 1) != lua_tonumber(L, -1)) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "unexpected signal");
		return 2;
	}
	lua_settop(L, 1);
	return 1;
}
static void uv_onsignal (uv_signal_t *handle, int signum) {
	lua_State *thread = (lua_State *)handle->data;
	lua_pushinteger(thread, signum);
	lcuU_resumethrop((uv_handle_t *)handle, 1);
}
static int k_setupsignal (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_signal_t *signal = (uv_signal_t *)handle;
	int signum = checksignal(L, 1);
	int err = 0;
	if (loop) err = lcuT_armthrop(L, uv_signal_init(loop, signal));
	else if (signal->signum != signum) err = uv_signal_stop(signal);
	else return -1;  /* yield on success */
	if (err >= 0) err = uv_signal_start(signal, uv_onsignal, signum);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_awaitsig (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetthropk(L, UV_SIGNAL, sched, k_setupsignal, returnsignal, NULL);
}


/* ended [, errmsg] = system.execute (command [, arguments...]) */
static const char* getstrfield (lua_State *L, const char *field, int required) {
	const char* value = NULL;
	lua_getfield(L, 1, field);
	if (required || !lua_isnil(L, -1)) {
		value = lua_tostring(L, -1);
		if (value == NULL)
			luaL_error(L, "bad field %s (must be a string)", field);
	}
	lua_pop(L, 1);
	return value;
}
static int ismetatable (lua_State *L, const char *meta) {
	int res;
	luaL_getmetatable(L, meta);
	res = lua_rawequal(L, -1, -2);
	lua_pop(L, 1);
	return res;
}
static void getstreamfield (lua_State *L,
                            const char *field,
                            uv_stdio_container_t *stream,
                            int deffd) {
	if (lua_getfield(L, 1, field) == LUA_TNIL) {
		stream->data.fd = deffd;
		stream->flags = UV_INHERIT_FD;
	} else {
		stream->flags = UV_IGNORE;
		if (lua_getmetatable(L, -1)) {
			if (ismetatable(L, LUA_FILEHANDLE)) {
				luaL_Stream *lstream = (luaL_Stream *)lua_touserdata(L, -2);
				stream->data.fd = fileno(lstream->f);
				stream->flags = UV_INHERIT_FD;
			} else if (ismetatable(L, LCU_TCPACTIVECLS) ||
			           ismetatable(L, LCU_PIPEACTIVECLS)) {
				lcu_Object *obj = (lcu_Object *)lua_touserdata(L, -2);
				stream->data.stream = (uv_stream_t *)lcu_toobjhdl(obj);
				stream->flags = UV_INHERIT_STREAM;
			}
		}
		lua_pop(L, 1); /* remove value's metatable */
		if (stream->flags == UV_IGNORE)
			luaL_error(L, "bad field %s (must be a stream)", field);
	}
	lua_pop(L, 1); /* remove value */
}
static int getprocopts (lua_State *L, uv_process_options_t *procopts) {
	procopts->flags = 0;

	if (lua_isstring(L, 1)) {
		int i;
		int argc = lua_gettop(L);
		if (argc > LCU_EXECARGCOUNT) {
			size_t argsz = (argc+1)*sizeof(char *);  /* arguments + NULL */
			procopts->args = (char **)lua_newuserdatauv(L, argsz, 0);
		}
		for (i = 0; i < argc; i++)
			procopts->args[i] = (char *)luaL_checkstring(L, i+1);
		procopts->args[argc] = NULL;
		procopts->file = procopts->args[0];
		procopts->env = NULL;
		procopts->cwd = NULL;
		procopts->stdio_count = 3;
		for (i = 0; i < procopts->stdio_count; i++) {
			procopts->stdio[i].data.fd = i;
			procopts->stdio[i].flags = UV_INHERIT_FD;
		}

		return 0;
	} else if (lua_istable(L, 1)) {
		static const char *streamfields[] = { "stdin", "stdout", "stderr", NULL };
		size_t argc = 0, envc = 0;

		lua_settop(L, 1);  /* discard all other arguments */
		procopts->file = getstrfield(L, "execfile", 1);
		procopts->cwd = getstrfield(L, "runpath", 0);
		procopts->stdio_count = 0;
		for (; streamfields[procopts->stdio_count]; ++procopts->stdio_count)
			getstreamfield(L, streamfields[procopts->stdio_count],
			                  procopts->stdio+procopts->stdio_count,
			                  procopts->stdio_count);
		lua_getfield(L, 1, "arguments");
		lua_getfield(L, 1, "environment");

		if (lua_istable(L, 2)) {
			int i;
			lua_len(L, 2);  /* push arguments count */
			argc = 1+lua_tointeger(L, -1);  /* execfile + arguments */
			lua_pop(L, 1);  /* pop arguments count */
			if (argc > LCU_EXECARGCOUNT) {
				size_t argsz = (argc+1)*sizeof(char *);  /* arguments + NULL */
				procopts->args = (char **)lua_newuserdatauv(L, argsz, 0);
			}
			for (i = 1; i < argc; i++) {
				lua_geti(L, 2, i);
				procopts->args[i] = (char *)lua_tostring(L, -1);
				if (!procopts->args[i]) return luaL_error(L,
					"bad value #%d in field 'arguments' (must be a string)", i);
				lua_pop(L, 1);
			}
		} else if (!lua_isnil(L, 2)) {
			return luaL_argerror(L, 1, "field 'arguments' must be a table");
		}

		if (lua_istable(L, 3)) {
			void *mem;
			char **envl, *envv;
			size_t envsz = sizeof(char *);
			int i = 0;

			/* check environment variables are strings */
			lua_pushnil(L);  /* first key */
			while (lua_next(L, 3) != 0) {
				const char *name = lua_tostring(L, -2);
				const char *value = lua_tostring(L, -1);
				if (name && (value == NULL || strchr(name, '='))) return luaL_error(L,
					"bad name '%s' in field 'environment' (must be a string without '=')",
					name);
				envc++;
				envsz += sizeof(char *)+sizeof(char)*(strlen(name)+1+strlen(value)+1);
				lua_pop(L, 1);
			}

			mem = lua_newuserdatauv(L, envsz, 0);
			envl = (char **)mem;
			envv = (char *)(mem+(envc+1)*sizeof(char *));  /* variables + NULL */
			lua_pushnil(L);  /* first key */
			while (lua_next(L, 3) != 0) {
				const char *c = lua_tostring(L, -2);  /* variable name */
				envl[i++] = envv; /* put string in 'envl' array */
				while (*c) *envv++ = *c++; /* copy key to string, excluding '\0' */
				*envv++ = '=';
				c = lua_tostring(L, -1);  /* variable value */
				while ((*envv++ = *c++)); /* copy value to string, including '\0' */
				lua_pop(L, 1);
			}
			envl[i] = NULL; /* put NULL to mark the end of 'envl' array */
			procopts->env = envl;
		} else if (lua_isnil(L, 3)) {
			procopts->env = NULL;
		} else {
			return luaL_argerror(L, 1,
				"field 'environment' must be a table");
		}

		procopts->args[0] = (char *)procopts->file;
		procopts->args[argc] = NULL;

		return 1;
	}
	return luaL_argerror(L, 1, "table or string expected");
}
static void uv_procexited (uv_process_t *process, int64_t exitval, int signum) {
	lua_State *thread = (lua_State *)process->data;
	if (signum) {
		pushsignal(thread, signum);
	} else {
		lua_pushliteral(thread, "exit");
		lua_pushinteger(thread, exitval);
	}
	lcuU_resumethrop((uv_handle_t *)process, 2);
}
static int k_setupproc (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_process_t *process = (uv_process_t *)handle;
	uv_process_options_t procopts;
	uv_stdio_container_t streams[3];
	char *args[LCU_EXECARGCOUNT+1];  /* values + NULL */
	int err, tabarg;

	lcu_assert(loop);  /* must be an uninitialized handler */
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");

	procopts.exit_cb = uv_procexited;
	procopts.args = args;
	procopts.stdio = streams;
	tabarg = getprocopts(L, &procopts);

	err = uv_spawn(loop, process, &procopts);
	lcuT_armthrop(L, 0);  /* 'uv_spawn' always arms the operation */
	if (err < 0) return lcuL_pusherrres(L, err);
	if (tabarg) {
		lua_pushinteger(L, uv_process_get_pid(process));
		lua_setfield(L, 1, "pid");
	}
	return -1;  /* yield on success */
}
static int system_execute (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetthropk(L, -1, sched, k_setupproc, NULL, NULL);
}


LCUI_FUNC void lcuM_addsignalf (lua_State *L) {
	static const luaL_Reg luaf[] = {
		{"getpid", system_getpid},
		{"emitsig", system_emitsig},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"awaitsig", system_awaitsig},
		{"execute", system_execute},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, luaf, 0);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
