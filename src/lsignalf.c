#include "lmodaux.h"
#include "loperaux.h"
#include "lprocaux.h"

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


#define toproc(L)	lcu_checkprocess(L,1)


/* success [, errmsg] = system.emitsig (process, signal) */
static int system_emitsig (lua_State *L) {
	int signum = checksignal(L, 2);
	int err = lua_isinteger(L, 1) ? uv_kill(lua_tointeger(L, 1), signum)
	                              : uv_process_kill(lcu_toprochandle(toproc(L)), signum);
	return lcuL_pushresults(L, 0, err);
}


/* signal [, errmsg] = system.awaitsig(signal) */
static int returnsignal (lua_State *L) {
	if (checksignal(L, 1) != lua_tonumber(L, -1)) {
		lua_pushnil(L);
		lua_pushliteral(L, "unexpected signal");
		return 2;
	}
	lua_settop(L, 1);
	return 1;
}
static void uv_onsignal (uv_signal_t *handle, int signum) {
	lua_State *thread = (lua_State *)handle->data;
	lua_pushinteger(thread, signum);
	lcuU_resumethrop(thread, (uv_handle_t *)handle);
}
static int k_setupsignal (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_signal_t *signal = (uv_signal_t *)handle;
	int signum = checksignal(L, 1);
	int err = 0;
	if (loop) err = lcuT_armthrop(L, uv_signal_init(loop, signal));
	else if (signal->signum != signum) err = uv_signal_stop(signal);
	else return 0;
	if (err >= 0) err = uv_signal_start(signal, uv_onsignal, signum);
	return err;
}
static int system_awaitsig (lua_State *L) {
	return lcuT_resetthropk(L, UV_SIGNAL, k_setupsignal, returnsignal);
}


/* string = tostring(process) */
static int process_tostring (lua_State *L) {
	lcu_Process *process = toproc(L);
	if (!lcu_isprocclosed(process)) {
		uv_pid_t pid = uv_process_get_pid(lcu_toprochandle(process));
		lua_pushfstring(L, "process (%p)", pid);
	}
	else lua_pushliteral(L, "process (closed)");
	return 1;
}


/* getmetatable(process).__gc(process) */
static int process_gc (lua_State *L) {
	lcu_closeproc(L, 1);
	return 0;
}


/* succ [, errmsg] = process:close() */
static int process_close (lua_State *L) {
	lcu_Process *process = toproc(L);
	if (!lcu_isprocclosed(process)) {
		int closed = lcu_closeproc(L, 1);
		lcu_assert(closed);
		lua_pushboolean(L, closed);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/* status, ... = process:status () */
static int process_status (lua_State *L) {
	lcu_Process *process = toproc(L);
	if (!lcu_isprocclosed(process)) {
		int64_t exitval;
		int signum;
		if (lcu_getprocexited(process, &exitval, &signum)) {
			lua_pushliteral(L, "dead");
			lua_pushinteger(L, exitval);
			pushsignal(L, signum);
			return 4;
		}
		lua_pushliteral(L, "running");
		lua_pushinteger(L, uv_process_get_pid(lcu_toprochandle(process)));
		return 2;
	}
	lua_pushliteral(L, "closed");
	return 1;
}


/* process [, errmsg] = system.execute (command [, arguments...]) */
static const char* getstrfield(lua_State *L, const char *field, int required) {
	const char* value = NULL;
	lua_getfield(L, 1, field);
	if (required || !lua_isnil(L, -1)) {
		value = lua_tostring(L, -1);
		if (value == NULL)
			luaL_error(L, "bad field "LUA_QS" (must be a string)", field);
	}
	lua_pop(L, 1);
	return value;
}
//static int optstreamfield(lua_State *L,
//                          const char *field,
//                          losi_ProcStream *stream) {
//	int res = 0;
//	lua_getfield(L, 1, field);
//	if (!lua_isnil(L, -1)) {
//		if (!losi_getprocstrm(L, -1, stream))
//			luaL_error(L, "bad field "LUA_QS" (must be a stream)", field);
//		res = 1;
//	}
//	lua_pop(L, 1); /* remove value */
//	return res;
//}
static void uv_procexited (uv_process_t *prochdl, int64_t exitval, int signum) {
	lcu_Process *process = (lcu_Process *)prochdl;
	lua_State *thread = (lua_State *)prochdl->data;
	lcu_setprocexited(process, exitval, signum);
	if (thread) {
		lua_pushinteger(thread, exitval);
		pushsignal(thread, signum);
		lcuU_resumeobjop(thread, (uv_handle_t *)prochdl);
	}
}
static lua_Integer getlistlen (lua_State *L, int idx) {
	lua_Integer value;
	lua_len(L, idx);
	value = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return value;
}
static int system_execute (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_Process *process = lcu_newprocess(L);
	uv_process_t *prochdl = lcu_toprochandle(process);
	uv_process_options_t procopts;
	uv_stdio_container_t streams[3];
	char *args[LCU_EXECARGCOUNT+1];  /* values + NULL */
	size_t argsz = 0, envsz = 0;
	int err;

	procopts.exit_cb = uv_procexited;
	procopts.flags = 0;
	procopts.args = args;
	procopts.stdio_count = 0;
	procopts.stdio = streams;

	if (lua_isstring(L, 1)) {
		int i;
		int argc = lua_gettop(L)-1;  /* ignore process userdata on top */
		for (i = 1; i <= argc; ++i) luaL_checkstring(L, i);
		if (argc > LCU_EXECARGCOUNT) {
			argsz = (argc+1)*sizeof(char *);  /* arguments + NULL */
			procopts.args = (char **)lcuL_allocmemo(L, argsz);
		}
		for (i = 0; i < argc; ++i) procopts.args[i] = (char *)lua_tostring(L, i+1);
		procopts.args[argc] = NULL;
		procopts.file = procopts.args[0];
		procopts.env = NULL;
		procopts.cwd = NULL;
	} else if (lua_istable(L, 1)) {
		size_t argc = 0, envc = 0;

		lua_replace(L, 2);  /* place userdata at index 2 */
		lua_settop(L, 2);  /* discard all other arguments */
		procopts.file = getstrfield(L, "execfile", 1);
		procopts.cwd = getstrfield(L, "runpath", 0);
		lua_getfield(L, 1, "arguments");
		lua_getfield(L, 1, "environment");

		if (lua_istable(L, 3)) {
			int i;
			argc = 1+getlistlen(L, 3);  /* execfile + arguments */
			/* check arguments are strings */
			for (i = 1; i < argc; ++i) {
				lua_geti(L, 3, i);
				if (!lua_isstring(L, -1)) luaL_error(L,
					"bad value #%d in field "LUA_QL("arguments")" (must be a string)", i);
				lua_pop(L, 1); /* pop an argument string */
			}
		} else if (!lua_isnil(L, 3)) {
			luaL_argerror(L, 1, "field "LUA_QL("arguments")" must be a table");
		}

		if (lua_istable(L, 4)) {
			void *mem;
			char **envl, *envv;
			int i = 0;

			/* check environment variables are strings */
			envsz = sizeof(char *);
			lua_pushnil(L);  /* first key */
			while (lua_next(L, 4) != 0) {
				const char *name = lua_tostring(L, -2);
				const char *value = lua_tostring(L, -1);
				if (name && (value == NULL || strchr(name, '=')))
					luaL_error(L, "bad name "LUA_QS" in field "LUA_QL("environment")
					              " (must be a string without "LUA_QL("=")")", name);
				++envc;
				envsz += sizeof(char *)+sizeof(char)*(strlen(name)+1+strlen(value)+1);
				lua_pop(L, 1);
			}

			mem = lcuL_allocmemo(L, envsz);
			if (mem == NULL) luaL_error(L, "insuffient memory");
			envl = (char **)mem;
			envv = (char *)(mem+(envc+1)*sizeof(char *));  /* variables + NULL */
			lua_pushnil(L);  /* first key */
			while (lua_next(L, 4) != 0) {
				const char *c = lua_tostring(L, -2);  /* variable name */
				envl[i++] = envv; /* put string in 'envl' array */
				while (*c) *envv++ = *c++; /* copy key to string, excluding '\0' */
				*envv++ = '=';
				c = lua_tostring(L, -1);  /* variable value */
				while ((*envv++ = *c++)); /* copy value to string, including '\0' */
				lua_pop(L, 1);
			}
			envl[i] = NULL; /* put NULL to mark the end of 'envl' array */
			procopts.env = envl;
		} else if (lua_isnil(L, 4)) {
			procopts.env = NULL;
		} else {
			luaL_argerror(L, 1, "field "LUA_QL("environment")" must be a table");
		}

		if (lua_istable(L, 3)) {
			int i;
			if (argc > LCU_EXECARGCOUNT) {
				argsz = (argc+1)*sizeof(char *);  /* arguments + NULL */
				procopts.args = (char **)lcuL_allocmemo(L, argsz);
				if (procopts.args == NULL) {
					if (procopts.env != NULL) lcuL_freememo(L, procopts.env, envsz);
					luaL_error(L, "insuffient memory");
				}
			}
			for (i = 1; i < argc; ++i) {
				lua_geti(L, 3, i);
				procopts.args[i] = (char *)lua_tostring(L, -1);
				lua_pop(L, 1);
			}
		}
		procopts.args[0] = (char *)procopts.file;
		procopts.args[argc] = NULL;
		lua_pushvalue(L, 2);
	} else {
		return luaL_argerror(L, 1, "table or string expected");
	}

	err = uv_spawn(loop, prochdl, &procopts);
	if (procopts.args != args) lcuL_freememo(L, procopts.args, argsz);
	if (procopts.env != NULL) lcuL_freememo(L, procopts.env, envsz);
	if (!err) {
		prochdl->data = NULL;
		lcu_enableproc(L, -1);
	}
	return lcuL_pushresults(L, 1, err);
}


/* exitval, signal = process:awaitexit () */
static int k_procexited (lua_State *L, int status, lua_KContext ctx) {
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	return lua_gettop(L)-1;
}
static int process_awaitexit (lua_State *L) {
	lcu_Process *process = toproc(L);
	int64_t exitval;
	int signum;
	if (lcu_getprocexited(process, &exitval, &signum)) {
		lua_pushinteger(L, exitval);
		pushsignal(L, signum);
		return 3;
	} else {
		uv_process_t *prochdl = lcu_toprochandle(process);
		luaL_argcheck(L, !lcu_isprocclosed(process), 1, "closed");
		luaL_argcheck(L, prochdl->loop == lcu_toloop(L), 1, "foreign object");
		if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
		if (prochdl->data) luaL_argcheck(L, prochdl->data == L, 1, "already in use");
		else lcuT_awaitobj(L, (uv_handle_t *)prochdl);
		lua_settop(L, 1);
		return lua_yieldk(L, 0, 0, k_procexited);
	}
}


static const luaL_Reg proc[] = {
	{"__tostring", process_tostring},
	{"__gc", process_gc},
	{"close", process_close},
	{"status", process_status},
	{"awaitexit", process_awaitexit},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"awaitsig", system_awaitsig},
	{"emitsig", system_emitsig},
	{"execute", system_execute},
	{NULL, NULL}
};

LCULIB_API void lcuM_addsignalf (lua_State *L) {
	lcuM_newclass(L, proc, LCU_MODUPVS, LCU_PROCESSCLS, NULL);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
