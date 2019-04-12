#include "lmodaux.h"
#include "lhndlaux.h"

#include <string.h>
#include <signal.h>


static const struct { const char *name; int value; } signals[] = {

	{ "abort", SIGABRT },
	{ "bgread", SIGTTIN },
	{ "bgwrite", SIGTTOU },
	{ "child", SIGCHLD },
	{ "clocktime", SIGALRM },
	{ "continue", SIGCONT },
	{ "cpulimit", SIGXCPU },
#ifdef SIGPROF
	{ "cputimall", SIGPROF },
#endif
#ifdef SIGVTALRM
	{ "cputimprc", SIGVTALRM },
#endif
	{ "debug", SIGTRAP },
	{ "filelimit", SIGXFSZ },
	{ "hangup", SIGHUP },
	{ "interrupt", SIGINT },
	{ "loosepipe", SIGPIPE },
#ifdef SIGPOLL
	{ "polling", SIGPOLL },
#endif
	{ "quit", SIGQUIT },
	{ "stop", SIGTSTP },
#ifdef SIGSYS
	{ "sysargerr", SIGSYS },
#endif
	{ "terminate", SIGTERM },
	{ "urgsock", SIGURG },
	{ "user1", SIGUSR1 },
	{ "user2", SIGUSR2 },
#ifdef SIGWINCH
	{ "winresize", SIGWINCH },
#endif
	{ NULL, 0 },
};

static void pushsignal (lua_State *L, int signum) {
	int i;
	for (i=0; signals[i].name; i++) {
		if (signals[i].value == signum) {
			lua_pushstring(L, signals[i].name);
			return;
		}
	}
	lua_pushnil(L);
}

static int checksignal (lua_State *L, int arg, const char *def) {
	const char *name = (def) ? luaL_optstring(L, arg, def) :
	                           luaL_checkstring(L, arg);
	int i;
	for (i=0; signals[i].name; i++)
		if (strcmp(signals[i].name, name) == 0)
			return signals[i].value;
	return luaL_argerror(L, arg,
	                     lua_pushfstring(L, "invalid signal '%s'", name));
}

static void lcuB_onsignal (uv_signal_t *h, int signum) {
	lua_State *co = (lua_State *)h->data;
	lcu_assert(co != NULL);
	lua_settop(co, 0);
	pushsignal(co, signum);
	lcu_resumehdl((uv_handle_t *)h, co);
}

static int lcuK_setupsignal (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	uv_handle_t *h = lcu_tohandle(L);
	uv_signal_t *signal = (uv_signal_t *)h;
	int signum = (int)ctx;
	lcu_chkinithdl(L, h, uv_signal_init(loop, signal));
	lcu_chkstarthdl(L, h, uv_signal_start(signal, lcuB_onsignal, signum));
	lua_remove(L, 1);
	return lcu_yieldhdl(L, lua_gettop(L), 0, lcuK_chkcancelhdl, h);
}

static int lcuM_awaitsig (lua_State *L) {
	int signum = checksignal(L, 1, NULL);
	int narg = lua_gettop(L)-1;
	uv_handle_t *h = lcu_resethdl(L, UV_SIGNAL, narg, (lua_KContext)signum, lcuK_setupsignal);
	uv_signal_t *signal = (uv_signal_t *)h;
	if (signal->signum != signum) {
		lcu_chkerror(L, uv_signal_stop(signal));
		lcu_chkstarthdl(L, h, uv_signal_start(signal, lcuB_onsignal, signum));
	}
	lua_remove(L, 1);
	return lcu_yieldhdl(L, narg, 0, lcuK_chkcancelhdl, h);
}

LCULIB_API void lcuM_addsignalf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"awaitsig", lcuM_awaitsig},
		{NULL, NULL}
	};
	lcuM_addmodfunc(L, modf);
}
