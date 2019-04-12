#include "lmodaux.h"
#include "lhndlaux.h"


static void lcuB_onidle (uv_idle_t *h) {
	lcu_resumehandle((uv_handle_t *)h, (lua_State *)h->data);
}

static int lcuK_setupidle (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	uv_handle_t *h = lcu_tohandle(L);
	uv_idle_t *idle = (uv_idle_t *)h;
	lcu_checkinit(L, h, uv_idle_init(loop, idle));
	lcu_checkstart(L, h, uv_idle_start(idle, lcuB_onidle));
	lua_remove(L, 1);
	return lcu_yieldk(L, lua_gettop(L), 0, lcuK_checkcancel, h);
}

static void lcuB_ontimer (uv_timer_t *h) {
	lcu_resumehandle((uv_handle_t *)h, (lua_State *)h->data);
}

static int lcuK_setuptimer (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	uv_handle_t *h = lcu_tohandle(L);
	uv_timer_t *timer = (uv_timer_t *)h;
	uint64_t msecs = (uint64_t)ctx;
	lcu_checkinit(L, h, uv_timer_init(loop, timer));
	lcu_checkstart(L, h, uv_timer_start(timer, lcuB_ontimer, msecs, msecs));
	lua_remove(L, 1);
	return lcu_yieldk(L, lua_gettop(L), 0, lcuK_checkcancel, h);
}

static int lcuM_pause (lua_State *L) {
	lua_Number delay;
	uv_handle_t *h;
	int narg = lua_gettop(L);
	if (narg == 0) lua_settop(L, 1);
	delay = luaL_optnumber(L, 1, 0);
	if (delay > 0) {
		uint64_t msecs = (uint64_t)(delay*1000);
		uv_timer_t *timer;
		h = lcu_resethandle(L, UV_TIMER, narg, (lua_KContext)msecs, lcuK_setuptimer);
		timer = (uv_timer_t *)h;
		if (uv_timer_get_repeat(timer) != msecs) {
			lcu_checkerr(L, uv_timer_stop(timer));
			lcu_checkstart(L, h, uv_timer_start(timer, lcuB_ontimer, msecs, msecs));
		}
	}
	else h = lcu_resethandle(L, UV_IDLE, narg, 0, lcuK_setupidle);
	lua_remove(L, 1);
	return lcu_yieldk(L, narg, 0, lcuK_checkcancel, h);
}


LCULIB_API void lcuM_addtimef (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"pause", lcuM_pause},
		{NULL, NULL}
	};
	lcuM_addmodfunc(L, modf);
}
