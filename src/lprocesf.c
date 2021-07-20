#include "lmodaux.h"
#include "loperaux.h"
#include "lsckdefs.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>


#ifdef SIGSTOP
#define CATCHABLE_INDEX 2
#else
#define CATCHABLE_INDEX 1
#endif

static const struct { const char *name; int value; } signals[] = {
	{ "TERMINATE", SIGKILL },
#ifdef SIGSTOP
	{ "STOP", SIGSTOP },
#endif
#ifdef SIGINT
	{ "interrupt", SIGINT },
#endif
#ifdef SIGTERM
	{ "terminate", SIGTERM },
#endif
#ifdef SIGTSTP
	{ "stop", SIGTSTP },
#endif
#ifdef SIGCONT
	{ "continue", SIGCONT },
#endif
#ifdef SIGHUP
	{ "hangup", SIGHUP },
#endif
#ifdef SIGTTIN
	{ "stdinoff", SIGTTIN },
#endif
#ifdef SIGTTOU
	{ "stdoutoff", SIGTTOU },
#endif
#ifdef SIGWINCH
	{ "winresize", SIGWINCH },
#endif
#ifdef SIGQUIT
	{ "quit", SIGQUIT },
#endif
#ifdef SIGCHLD
	{ "child", SIGCHLD },
#endif
#ifdef SIGABRT
	{ "abort", SIGABRT },
#endif
#ifdef SIGPIPE
	{ "brokenpipe", SIGPIPE },
#endif
#ifdef SIGURG
	{ "urgentsock", SIGURG },
#endif
#ifdef SIGUSR1
	{ "userdef1", SIGUSR1 },
#endif
#ifdef SIGUSR2
	{ "userdef2", SIGUSR2 },
#endif
#ifdef SIGTRAP
	{ "trap", SIGTRAP },
#endif
#ifdef SIGXFSZ
	{ "filelimit", SIGXFSZ },
#endif
#ifdef SIGALRM
	{ "clocktime", SIGALRM },
#endif
#ifdef SIGXCPU
	{ "cpulimit", SIGXCPU },
#endif
#ifdef SIGPROF
	{ "cputotal", SIGPROF },
#endif
#ifdef SIGVTALRM
	{ "cputime", SIGVTALRM },
#endif
#ifdef SIGPOLL
	{ "polling", SIGPOLL },
#endif
#ifdef SIGSYS
	{ "sysargerr", SIGSYS },
#endif
#ifdef SIGBREAK
	{ "break", SIGBREAK },
#endif
	{ NULL, 0 }
};

static void pushsignal (lua_State *L, int signum) {
	int i;
	for (i=0; signals[i].name && (signals[i].value != signum); i++);
	if (signals[i].name) lua_pushstring(L, signals[i].name);
	else lua_pushliteral(L, "signal");
	lua_pushinteger(L, signum);
}

static int checksignal (lua_State *L, int arg, int catch) {
	const char *name = luaL_checkstring(L, arg);
	int i;
	for (i = catch ? CATCHABLE_INDEX : 0; signals[i].name; i++)
		if (strcmp(signals[i].name, name) == 0)
			return signals[i].value;
	return luaL_argerror(L, arg,
	                     lua_pushfstring(L, "invalid signal '%s'", name));
}


#define checkpid(L,A)	((uv_pid_t)luaL_checkinteger(L,A))

/* success [, errmsg] = system.emitsig (process, signal) */
static int system_emitsig (lua_State *L) {
	uv_pid_t pid = checkpid(L, 1);
	lua_Integer signal = lua_isinteger(L, 2) ? lua_tointeger(L, 2)
	                                         : checksignal(L, 2, 0);
	int err = uv_kill(pid, signal);
	return lcuL_pushresults(L, 0, err);
}


/* signal [, errmsg] = system.awaitsig(signal) */
static int returnsignal (lua_State *L) {
	if (checksignal(L, 1, 1) != lua_tonumber(L, -1)) {
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
	lcuU_resumecohdl((uv_handle_t *)handle, 1);
}
static int k_setupsignal (lua_State *L,
                          uv_handle_t *handle,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	uv_signal_t *signal = (uv_signal_t *)handle;
	int signum = checksignal(L, 1, 1);
	int err = 0;
	if (loop) err = lcuT_armcohdl(L, op, uv_signal_init(loop, signal));
	else if (signal->signum != signum) err = uv_signal_stop(signal);
	else return -1;  /* yield on success */
	if (err >= 0) err = uv_signal_start(signal, uv_onsignal, signum);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_awaitsig (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetcohdlk(L, UV_SIGNAL, sched, k_setupsignal, returnsignal, NULL);
}


/* string, number = system.getpriority (pid) */
static int system_getpriority (lua_State *L) {
	uv_pid_t pid = checkpid(L, 1);
	int value;
	int err = uv_os_getpriority(pid, &value);
	if (err < 0) return lcuL_pusherrres(L, err);
	switch (value) {
		case UV_PRIORITY_HIGHEST: lua_pushliteral(L, "highest"); break;
		case UV_PRIORITY_HIGH: lua_pushliteral(L, "high"); break;
		case UV_PRIORITY_ABOVE_NORMAL: lua_pushliteral(L, "above"); break;
		case UV_PRIORITY_NORMAL: lua_pushliteral(L, "normal"); break;
		case UV_PRIORITY_BELOW_NORMAL: lua_pushliteral(L, "below"); break;
		case UV_PRIORITY_LOW: lua_pushliteral(L, "low"); break;
		default: lua_pushliteral(L, "other"); break;
	}
	lua_pushinteger(L, value);
	return 2;
}

/* true = system.setpriority (pid, value) */
static int system_setpriority (lua_State *L) {
	uv_pid_t pid = checkpid(L, 1);
	int priority, err;
	if (lua_isinteger(L, 2)) {
		lua_Integer value = luaL_checkinteger(L, 2);
		luaL_argcheck(L, -20 <= value && value < 20, 2, "out of range");
		priority = (int)value;
	} else {
		static const char *const options[] = { "highest", "high", "above",
		                                       "normal", "below", "low", NULL };
		int option = luaL_checkoption(L, 2, NULL, options);
		switch (option) {
			case 0: priority = UV_PRIORITY_HIGHEST; break;
			case 1: priority = UV_PRIORITY_HIGH; break;
			case 2: priority = UV_PRIORITY_ABOVE_NORMAL; break;
			case 3: priority = UV_PRIORITY_NORMAL; break;
			case 4: priority = UV_PRIORITY_BELOW_NORMAL; break;
			default: priority = UV_PRIORITY_LOW; break;
		}
	}
	err = uv_os_setpriority(pid, priority);
	return lcuL_pushresults(L, 0, err);
}


/* path = system.getdir () */
static int system_getdir (lua_State *L) {
	lcu_pushstrout(L, uv_cwd);
	return 1;
}

/* true = system.setdir (path) */
static int system_setdir (lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	int err = uv_chdir(path);
	return lcuL_pushresults(L, 0, err);
}


/* value = system.getenv ([name]) */
static int environ2table (lua_State *L) {
	uv_env_item_t *envlist = (uv_env_item_t *)lua_touserdata(L, 2);
	int envcount = (int)lua_tointeger(L, 3);
	lua_settop(L, 1);
	while (envcount--) {
		lua_pushstring(L, envlist[envcount].name);
		lua_pushstring(L, envlist[envcount].value);
		lua_settable(L, 1);
	}
	return 1;
}
static int system_getenv (lua_State *L) {
	switch (lua_type(L, 1)) {
		case LUA_TNONE:
		case LUA_TTABLE: {
			uv_env_item_t *envlist;
			int envcount;
			int err = uv_os_environ(&envlist, &envcount);
			if (err < 0) return lcuL_pusherrres(L, err);
			lua_pushcfunction(L, environ2table);
			if (lua_gettop(L) > 1) {
				lua_settop(L, 2);
				lua_insert(L, 1);
			} else {
				lua_createtable(L, 0, envcount);
			}
			lua_pushlightuserdata(L, envlist);
			lua_pushinteger(L, envcount);
			err = lua_pcall(L, 3, 1, 0);
			uv_os_free_environ(envlist, envcount);
			if (err) return lua_error(L);
		} break;
		default: {
			const char *name = luaL_checkstring(L, 1);
			char array[256];
			char *buffer = array;
			size_t len = sizeof(array);
			int err = uv_os_getenv(name, buffer, &len);
			if (err == UV_ENOBUFS) {
				buffer = (char *)malloc(len*sizeof(char));
				err = uv_os_getenv(name, buffer, &len);
			}
			if (err >= 0) lua_pushlstring(L, buffer, len);
			if (buffer != array) free(buffer);
			if (err < 0) return lcuL_pusherrres(L, err);
		}
	}
	return 1;
}


/* true = system.setenv (name, value) */
static int system_setenv (lua_State *L) {
	const char *name = luaL_checkstring(L, 1);
	int err = lua_isnil(L, 2) ? uv_os_unsetenv(name)
	                          : uv_os_setenv(name, luaL_checkstring(L, 2));
	return lcuL_pushresults(L, 0, err);
}


static char **newprocenv (lua_State *L, int idx) {
	void *mem;
	char **envl, *envv;
	size_t envc = 0, envsz = sizeof(char *);
	int i = 0;

	/* check environment variables are strings */
	lua_pushnil(L);  /* first key */
	while (lua_next(L, idx) != 0) {
		const char *name = lua_tostring(L, -2);
		const char *value = lua_tostring(L, -1);
		if (name && (value == NULL || strchr(name, '='))) luaL_error(L,
			"bad name '%s' in environment (cannot contain '=')", name);
		envc++;
		envsz += sizeof(char *)+sizeof(char)*(strlen(name)+1+strlen(value)+1);
		lua_pop(L, 1);
	}

	mem = lua_newuserdatauv(L, envsz, 0);
	envl = (char **)mem;
	envv = ((char *)mem)+(envc+1)*sizeof(char *);  /* variables + NULL */
	lua_pushnil(L);  /* first key */
	while (lua_next(L, idx) != 0) {
		const char *c = lua_tostring(L, -2);  /* variable name */
		envl[i++] = envv; /* put string in 'envl' array */
		while (*c) *envv++ = *c++; /* copy key to string, excluding '\0' */
		*envv++ = '=';
		c = lua_tostring(L, -1);  /* variable value */
		while ((*envv++ = *c++)); /* copy value to string, including '\0' */
		lua_pop(L, 1);
	}
	envl[i] = NULL; /* put NULL to mark the end of 'envl' array */
	return envl;
}

/* env = system.packenv (tab) */
static int system_packenv (lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	newprocenv(L, 1);
	luaL_setmetatable(L, LCU_PROCENVCLS);
	return 1;
}

/* tab = system.unpackenv (env [, tab]) */
static int system_unpackenv (lua_State *L) {
	const char **envl = (const char **)luaL_checkudata(L, 1, LCU_PROCENVCLS);
	if (lua_gettop(L) == 1) lua_newtable(L);
	else luaL_checktype(L, 2, LUA_TTABLE);
	for (; *envl; envl++) {
		const char *value = strchr(*envl, '=');
		lua_pushlstring(L, *envl, value-*envl);
		lua_pushstring(L, ++value);
		lua_settable(L, 2);
	}
	return 1;
}

/* value = env[name] */
static int procenv_index (lua_State *L) {
	const char **envl = (const char **)luaL_checkudata(L, 1, LCU_PROCENVCLS);
	size_t len;
	const char *name = luaL_checklstring(L, 2, &len);
	luaL_argcheck(L, !strchr(name, '='), 2, "cannot contain '='");
	for (; *envl; envl++) {
		if (strncmp(name, *envl, len) == 0 && (*envl)[len] == '=') {
			lua_pushstring(L, *envl+len+1);
			return 1;
		}
	}
	return 0;
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
                            uv_loop_t *loop,
                            const char *field,
                            uv_stdio_container_t *stream,
                            int deffd) {
	int ltype = lua_getfield(L, 1, field);
	if (ltype == LUA_TNIL) {
		stream->data.fd = deffd;
		stream->flags = UV_INHERIT_FD;
	} else if (ltype == LUA_TSTRING) {
		const char *mode = lua_tostring(L, -1);
		int socktranf = 0, err;
		lcu_PipeSocket *pipe;
		stream->flags = UV_CREATE_PIPE;
		for (; *mode; mode++) switch (*mode) {
			case 'w': stream->flags |= UV_READABLE_PIPE; break;
			case 'r': stream->flags |= UV_WRITABLE_PIPE; break;
			case 's': socktranf = 1; break;
			default: luaL_error(L, "unknown '%s' mode char (got '%c')", field, *mode);
		}
		pipe = lcuT_newudhdl(L, lcu_PipeSocket, socktranf ? LCU_PIPESHARECLS
		                                                  : LCU_PIPEACTIVECLS);
		stream->data.stream = (uv_stream_t *)lcu_ud2hdl(pipe);
		err = uv_pipe_init(loop, lcu_ud2hdl(pipe), socktranf);
		if (err) lcu_error(L, err);
		pipe->flags = socktranf ? LCU_SOCKTRANFFLAG : 0;
		lua_setfield(L, 1, field);
	} else {
		stream->flags = UV_IGNORE;
		if (ltype == LUA_TBOOLEAN) {
			if (lua_toboolean(L, -1))
				luaL_error(L, "bad field %s (got true)", field);
		} else if (lua_getmetatable(L, -1)) {
			if (ismetatable(L, LUA_FILEHANDLE)) {
				luaL_Stream *lstream = (luaL_Stream *)lua_touserdata(L, -2);
				stream->data.fd = fileno(lstream->f);
				stream->flags = UV_INHERIT_FD;
			} else if (ismetatable(L, LCU_TCPACTIVECLS) ||
			           ismetatable(L, LCU_PIPEACTIVECLS)) {
				lcu_UdataHandle *obj = (lcu_UdataHandle *)lua_touserdata(L, -2);
				stream->data.stream = (uv_stream_t *)lcu_ud2hdl(obj);
				stream->flags = UV_INHERIT_STREAM;
			}
			lua_pop(L, 1); /* remove value's metatable */
			if (stream->flags == UV_IGNORE)
				luaL_error(L, "bad field %s (invalid stream)", field);
		}
	}
	lua_pop(L, 1); /* remove value */
}
static int getprocopts (lua_State *L,
                        uv_loop_t *loop,
                        uv_process_options_t *procopts) {
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
		int argc = 0;

		lua_settop(L, 1);  /* discard all other arguments */
		procopts->file = getstrfield(L, "execfile", 1);
		procopts->cwd = getstrfield(L, "runpath", 0);
		procopts->stdio_count = 0;
		for (; streamfields[procopts->stdio_count]; ++procopts->stdio_count)
			getstreamfield(L, loop, streamfields[procopts->stdio_count],
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

		if (lua_isnoneornil(L, 3)) {
			procopts->env = NULL;
		} else if (lua_istable(L, 3)) {
			procopts->env = newprocenv(L, 3);
		} else {
			procopts->env = (char **)luaL_checkudata(L, 3, LCU_PROCENVCLS);
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
	lcuU_resumecohdl((uv_handle_t *)process, 2);
}
static int k_setupproc (lua_State *L,
                        uv_handle_t *handle,
                        uv_loop_t *loop,
                        lcu_Operation *op) {
	uv_process_t *process = (uv_process_t *)handle;
	uv_process_options_t procopts;
	uv_stdio_container_t streams[3];
	char *args[LCU_EXECARGCOUNT+1];  /* values + NULL */
	int err, tabarg;

	lcu_assert(loop);  /* must be an uninitialized handler */

	procopts.exit_cb = uv_procexited;
	procopts.args = args;
	procopts.stdio = streams;
	tabarg = getprocopts(L, loop, &procopts);

	err = uv_spawn(loop, process, &procopts);
	lcuT_armcohdl(L, op, 0);  /* 'uv_spawn' always arms the operation */
	if (err < 0) return lcuL_pusherrres(L, err);
	if (tabarg) {
#if LCU_LIBUVMINVER(19)
		lua_pushinteger(L, uv_process_get_pid(process));
		lua_setfield(L, 1, "pid");
#endif
	}
	return -1;  /* yield on success */
}
static int system_execute (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetcohdlk(L, -1, sched, k_setupproc, NULL, NULL);
}


LCUI_FUNC void lcuM_addprocesf (lua_State *L) {
	static const luaL_Reg envmt[] = {
		{"__index", procenv_index},
		{NULL, NULL}
	};
	static const luaL_Reg luaf[] = {
		{"getpriority", system_getpriority},
		{"setpriority", system_setpriority},
		{"getdir", system_getdir},
		{"setdir", system_setdir},
		{"getenv", system_getenv},
		{"setenv", system_setenv},
		{"packenv", system_packenv},
		{"unpackenv", system_unpackenv},
		{"emitsig", system_emitsig},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"awaitsig", system_awaitsig},
		{"execute", system_execute},
		{NULL, NULL}
	};

	luaL_newmetatable(L, LCU_PROCENVCLS);
	luaL_setfuncs(L, envmt, 0);
	lua_pop(L, 1);

	lcuM_setfuncs(L, luaf, 0);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
