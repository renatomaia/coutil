#include "lmodaux.h"
#include "loperaux.h"


static int returntrue (lua_State *L) {
	lua_pushboolean(L, 1);
	return 1;
}

static void onidle (uv_idle_t *handle) {
	lcuU_resumethrop((lua_State *)handle->data, (uv_handle_t *)handle);
}

static int setupidle (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_idle_t *idle = (uv_idle_t *)handle;
	int err = 0;
	if (loop) {
		err = lcuT_armthrop(L, uv_idle_init(loop, idle));
		if (err >= 0) err = uv_idle_start(idle, onidle);
	}
	return err;
}

static void ontimer (uv_timer_t *handle) {
	lcuU_resumethrop((lua_State *)handle->data, (uv_handle_t *)handle);
}

static int setuptimer (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_timer_t *timer = (uv_timer_t *)handle;
	uint64_t msecs = (uint64_t)(lua_tonumber(L, 1)*1000);
	int err = 0;
	if (loop) err = lcuT_armthrop(L, uv_timer_init(loop, timer));
	else if (uv_timer_get_repeat(timer) != msecs) err = uv_timer_stop(timer);
	else return 0;
	if (err >= 0) err = uv_timer_start(timer, ontimer, msecs, msecs);
	return err;
}

/* succ [, errmsg, ...] = system.pause([delay]) */
static int lcuM_pause (lua_State *L) {
	lua_Number delay = luaL_optnumber(L, 1, 0);
	if (delay > 0)
		return lcuT_resetopk(L, LCU_THROP, UV_TIMER, setuptimer, returntrue);
	else
		return lcuT_resetopk(L, LCU_THROP, UV_IDLE, setupidle, returntrue);
}


LCULIB_API void lcuM_addtimef (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"pause", lcuM_pause},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
