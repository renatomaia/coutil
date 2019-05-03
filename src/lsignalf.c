#include "lmodaux.h"
#include "loperaux.h"

#include <string.h>
#include <signal.h>


static int checksignal (lua_State *L, int arg, const char *def) {
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
	const char *name = luaL_checkstring(L, arg);
	int i;
	for (i=0; signals[i].name; i++)
		if (strcmp(signals[i].name, name) == 0)
			return signals[i].value;
	return luaL_argerror(L, arg,
	                     lua_pushfstring(L, "invalid signal '%s'", name));
}

static int endsignal (lua_State *L) {
	if (!lua_equal(L, 2, -1)) return lcuL_pusherror(L, "unexpected signal");
	lua_settop(L, 1);
	return 1;
}

static void onsignal (uv_signal_t *handle, int signum) {
	lua_State *thread = (lua_State *)handle->data;
	lua_pushinteger(thread, signum);
	lcuU_resumethrop(thread, handle);
}

static int setupsignal (lua_State *L,
                        lcu_Operation *op,
                        uv_handle_t *handle,
                        uv_loop_t *loop) {
	int signum = checksignal(L, 1);
	uv_signal_t *signal = (uv_signal_t *)handle;
	int err = 0;
	if (loop) err = lcuT_armthrop(L, op, uv_signal_init(loop, signal));
	else if (signal->signum != signum) err = uv_signal_stop(signal);
	if (err < 0) return err;
	return uv_signal_start(signal, onsignal, signum);
}

/* succ [, errmsg, ...] = system.awaitsig(signal) */
static int lcuM_awaitsig (lua_State *L) {
	return lcuT_resetopk(L, LCU_OPKIND_REQ, UV_SIGNAL, setupsignal, endsingal);
}

LCULIB_API void lcuM_addsignalf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"awaitsig", lcuM_awaitsig},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
