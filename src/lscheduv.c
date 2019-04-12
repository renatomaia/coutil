#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>

LUALIB_API void luaL_printstack(lua_State *L)
{
	int i;
	for(i = 1; i <= lua_gettop(L); ++i) {
		printf("%d = ", i);
		switch (lua_type(L, i)) {
			case LUA_TNUMBER:
				printf("%g", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("\"%s\"", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf(lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNIL:
				printf("nil");
				break;
			default:
				printf("%s: %p", luaL_typename(L, i),
				                 lua_topointer(L, i));
				break;
		}
		printf("\n");
	}
	printf("\n");
}

#include <signal.h>
#include <string.h>
#include <uv.h>


#if !defined(cos_assert)
#define cos_assert(X)	((void)(X))
#endif

#define COS_HANDLESMAP	lua_upvalueindex(3)
#define COS_COREGISTRY	lua_upvalueindex(2)
#define cosL_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(1))
#define cosL_error(L,e)	luaL_error(L, uv_strerror(e))
#define cosL_yieldk(L,n,c,f,h) ((h)->data = L, lua_yieldk(L, n, c, f))

static uv_handle_t *cosL_tohandle (lua_State *L) {
	uv_handle_t *h;
	lua_pushthread(L);
	if (lua_gettable(L, COS_HANDLESMAP) == LUA_TNIL) {
		lua_pushthread(L);
		h = (uv_handle_t *)lua_newuserdata(L, sizeof(union uv_any_handle));
		h->type = UV_UNKNOWN_HANDLE;
		h->data = NULL;
		lua_settable(L, COS_HANDLESMAP);
	}
	else h = (uv_handle_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return h;
}

static void cosL_refcoro (uv_handle_t *h, lua_State *L) {
	lua_pushlightuserdata(L, h);
	lua_pushthread(L);
	lua_settable(L, COS_COREGISTRY);
}

static void cosL_unrefcoro (uv_handle_t *h) {
	lua_State *L = (lua_State *)h->loop->data;
	lua_pushlightuserdata(L, h);
	lua_pushnil(L);
	lua_settable(L, COS_COREGISTRY);
}

static void cosL_checkerr (lua_State *L, int err) {
	if (err < 0) cosL_error(L, err);
}


/* Function 'run' (uv_loop_t) */

static int cosM_run (lua_State *L) {
	static const char *const opts[] = {"loop", "step", "ready", NULL};
	uv_loop_t *loop = cosL_toloop(L);
	int pending;
	int mode = luaL_checkoption(L, 1, "loop", opts);
	luaL_argcheck(L, !loop->data, 1, "already running");
	lua_settop(L, 2);  /* set trap function as top */
	loop->data = L;
	pending = uv_run(loop, mode);
	loop->data = NULL;
	lua_pushboolean(L, pending);
	return 1;
}


/* Scheduling auxiliary functions (uv_handle_t) */

static void cosL_resumehandle (uv_handle_t *h, lua_State *co);

static void cosB_onclosed (uv_handle_t *h) {
	h->type = UV_UNKNOWN_HANDLE;
	lua_State *co = (lua_State *)h->data;
	if (co != NULL) cosL_resumehandle(h, co);
	else cosL_unrefcoro(h);
}

static void cosL_checkinit (lua_State *L, uv_handle_t *h, int err) {
	cosL_checkerr(L, err);
	cosL_refcoro(h, L);
}

static void cosL_checkstart (lua_State *L, uv_handle_t *h, int err) {
	if (err < 0) {
		uv_close(h, cosB_onclosed);
		cosL_error(L, err);  /* never returns */
	}
}

static void cosL_resumehandle (uv_handle_t *h, lua_State *co) {
	lua_State *L = (lua_State *)h->loop->data;
	int status;
	h->data = NULL;  /* mark as not rescheduled */
	lua_pushlightuserdata(co, h->loop);  /* token to sign scheduler resume */
	status = lua_resume(co, L, 1);
	if ( (status != LUA_YIELD || h->data == NULL) &&  /* was not rescheduled */
	     h->type != UV_UNKNOWN_HANDLE &&  /* is still scheduled */
	     !uv_is_closing(h) ) uv_close(h, cosB_onclosed);
}

static uv_handle_t *cosL_resethandlek (lua_State *L, uv_handle_type t, int narg,
                                       lua_KContext ctx, lua_KFunction func) {
	uv_handle_t *h = cosL_tohandle(L);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (h->type == UV_UNKNOWN_HANDLE) {
		func(L, LUA_YIELD, ctx);  /* never returns */
	} else if ( h->type != t || uv_is_closing(h) ) {
		if (!uv_is_closing(h)) uv_close(h, cosB_onclosed);
		cosL_yieldk(L, narg, ctx, func, h);  /* never returns */
	}
	return h;
}

static int cosK_checkcancelled (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = cosL_toloop(L);
	uv_handle_t *h = cosL_tohandle(L);
	int narg = (int)ctx;
	int cancel = lua_touserdata(L, -1) != loop;
	cos_assert(status == LUA_YIELD);
	if (cancel && !uv_is_closing(h)) uv_close(h, cosB_onclosed);
	h->data = NULL;  /* mark as not rescheduled */
	lua_pushboolean(L, !cancel);
	lua_insert(L, narg+1);
	return lua_gettop(L)-narg;
}


/* Function 'pause' (uv_idle_t) */

static void cosB_onidle (uv_idle_t *h) {
	cosL_resumehandle((uv_handle_t *)h, (lua_State *)h->data);
}

static int cosK_setupidle (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = cosL_toloop(L);
	uv_handle_t *h = cosL_tohandle(L);
	uv_idle_t *idle = (uv_idle_t *)h;
	cosL_checkinit(L, h, uv_idle_init(loop, idle));
	cosL_checkstart(L, h, uv_idle_start(idle, cosB_onidle));
	lua_remove(L, 1);
	return cosL_yieldk(L, lua_gettop(L), 0, cosK_checkcancelled, h);
}

static void cosB_ontimer (uv_timer_t *h) {
	cosL_resumehandle((uv_handle_t *)h, (lua_State *)h->data);
}

static int cosK_setuptimer (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = cosL_toloop(L);
	uv_handle_t *h = cosL_tohandle(L);
	uv_timer_t *timer = (uv_timer_t *)h;
	uint64_t msecs = (uint64_t)ctx;
	cosL_checkinit(L, h, uv_timer_init(loop, timer));
	cosL_checkstart(L, h, uv_timer_start(timer, cosB_ontimer, msecs, msecs));
	lua_remove(L, 1);
	return cosL_yieldk(L, lua_gettop(L), 0, cosK_checkcancelled, h);
}

static int cosM_pause (lua_State *L) {
	lua_Number delay;
	uv_handle_t *h;
	int narg = lua_gettop(L);
	if (narg == 0) lua_settop(L, 1);
	delay = luaL_optnumber(L, 1, 0);
	if (delay > 0) {
		uint64_t msecs = (uint64_t)(delay*1000);
		uv_timer_t *timer;
		h = cosL_resethandlek(L, UV_TIMER, narg, (lua_KContext)msecs, cosK_setuptimer);
		timer = (uv_timer_t *)h;
		if (uv_timer_get_repeat(timer) != msecs) {
			cosL_checkerr(L, uv_timer_stop(timer));
			cosL_checkstart(L, h, uv_timer_start(timer, cosB_ontimer, msecs, msecs));
		}
	}
	else h = cosL_resethandlek(L, UV_IDLE, narg, 0, cosK_setupidle);
	lua_remove(L, 1);
	return cosL_yieldk(L, narg, 0, cosK_checkcancelled, h);
}


/* Function 'signal' (uv_signal_t) */

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

static void cosL_pushsignal (lua_State *L, int signum) {
	int i;
	for (i=0; signals[i].name; i++) {
		if (signals[i].value == signum) {
			lua_pushstring(L, signals[i].name);
			return;
		}
	}
	lua_pushnil(L);
}

static int cosL_checksignal (lua_State *L, int arg, const char *def) {
	const char *name = (def) ? luaL_optstring(L, arg, def) :
	                           luaL_checkstring(L, arg);
	int i;
	for (i=0; signals[i].name; i++)
		if (strcmp(signals[i].name, name) == 0)
			return signals[i].value;
	return luaL_argerror(L, arg,
	                     lua_pushfstring(L, "invalid signal '%s'", name));
}

static void cosB_onsignal (uv_signal_t *h, int signum) {
	lua_State *co = (lua_State *)h->data;
	cos_assert(co != NULL);
	lua_settop(co, 0);
	cosL_pushsignal(co, signum);
	cosL_resumehandle((uv_handle_t *)h, co);
}

static int cosK_setupsignal (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = cosL_toloop(L);
	uv_handle_t *h = cosL_tohandle(L);
	uv_signal_t *signal = (uv_signal_t *)h;
	int signum = (int)ctx;
	cosL_checkinit(L, h, uv_signal_init(loop, signal));
	cosL_checkstart(L, h, uv_signal_start(signal, cosB_onsignal, signum));
	lua_remove(L, 1);
	return cosL_yieldk(L, lua_gettop(L), 0, cosK_checkcancelled, h);
}

static int cosM_awaitsig (lua_State *L) {
	int signum = cosL_checksignal(L, 1, NULL);
	int narg = lua_gettop(L)-1;
	uv_handle_t *h = cosL_resethandlek(L, UV_SIGNAL, narg, (lua_KContext)signum, cosK_setupsignal);
	uv_signal_t *signal = (uv_signal_t *)h;
	if (signal->signum != signum) {
		cosL_checkerr(L, uv_signal_stop(signal));
		cosL_checkstart(L, h, uv_signal_start(signal, cosB_onsignal, signum));
	}
	lua_remove(L, 1);
	return cosL_yieldk(L, narg, 0, cosK_checkcancelled, h);
}


/* Module */

static void cosL_pushhandles (lua_State *L) {
	lua_pushlightuserdata(L, cosL_pushhandles);
	if (lua_gettable(L, LUA_REGISTRYINDEX) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushlightuserdata(L, cosL_pushhandles);
		lua_pushvalue(L, -2);
		lua_createtable(L, 0, 1);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
}

LUAMOD_API int luaopen_coutil_scheduler (lua_State *L)
{
	static const luaL_Reg modf[] = {
		{"run", cosM_run},
		{"pause", cosM_pause},
		{"awaitsig", cosM_awaitsig},
		{NULL, NULL}
	};
	luaL_newlibtable(L, modf);
	uv_loop_t *uv = (uv_loop_t *)lua_newuserdata(L, sizeof(uv_loop_t));
	cosL_checkerr(L, uv_loop_init(uv));
	lua_newtable(L); /* COS_COREGISTRY */
	cosL_pushhandles(L); /* COS_HANDLESMAP */
	luaL_setfuncs(L, modf, 3);
	return 1;
}
