#include "lmodaux.h"
#include "loperaux.h"


/* timestamp = system.nanosecs() */
static int system_nanosecs (lua_State *L) {
	lua_pushinteger(L, (lua_Integer)uv_hrtime());
	return 1;
}

/* timestamp = system.time([update]) */
static int system_time (lua_State *L) {
	static const char *const modes[] = {"cached","updated","epoch",NULL};
	int mode = luaL_checkoption(L, 1, "cached", modes);
	switch (mode) {
		case 0:
		case 1: {
			lcu_Scheduler *sched = lcu_getsched(L);
			uv_loop_t *loop = lcu_toloop(sched);
			if (mode) uv_update_time(loop);
			lua_pushnumber(L, (lua_Number)(uv_now(loop))/1e3);
		} break;
		case 2: {
			uv_timeval64_t time;
			int err = uv_gettimeofday(&time);
			if (err < 0) return lcuL_pusherrres(L, err);
			lua_pushnumber(L, lcu_time2sec(time));
		} break;
	}
	return 1;
}

/* succ [, errmsg] = system.suspend([delay]) */
static int returntrue (lua_State *L) {
	lua_pushboolean(L, 1);
	return 1;
}
static void uv_onidle (uv_idle_t *handle) {
	lcuU_resumecohdl((uv_handle_t *)handle, 0);
}
static int k_setupidle (lua_State *L,
                        uv_handle_t *handle,
                        uv_loop_t *loop,
                        lcu_Operation *op) {
	if (loop) {
		uv_idle_t *idle = (uv_idle_t *)handle;
		int err = lcuT_armcohdl(L, op, uv_idle_init(loop, idle));
		if (err >= 0) err = uv_idle_start(idle, uv_onidle);
		if (err < 0) return lcuL_pusherrres(L, err);
	}
	return -1;  /* yield on success */
}
static void uv_ontimer (uv_timer_t *handle) {
	lcuU_resumecohdl((uv_handle_t *)handle, 0);
}
static int k_setuptimer (lua_State *L,
                         uv_handle_t *handle,
                         uv_loop_t *loop,
                         lcu_Operation *op) {
	uv_timer_t *timer = (uv_timer_t *)handle;
	uint64_t msecs = (uint64_t)(lua_tonumber(L, 1)*1e3);
	int err;
	if (loop) err = lcuT_armcohdl(L, op, uv_timer_init(loop, timer));
	else if (uv_timer_get_repeat(timer) != msecs) err = uv_timer_stop(timer);
	else return -1;  /* yield on success */
	if (err >= 0) err = uv_timer_start(timer, uv_ontimer, msecs, msecs);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_suspend (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	lua_Number delay = luaL_optnumber(L, 1, 0);
	if (lcuL_checknoyieldmode(L, 2)) {
		if (delay > 0) {
			delay *= 1e3;
			luaL_argcheck(L, delay <= UINT_MAX, 1, "out of range");
			uv_sleep((unsigned int)delay);
		}
	} else if (delay > 0) {
		luaL_argcheck(L, delay*1e3 <= 0xffffffffffffffff, 1, "out of range");
		return lcuT_resetcohdlk(L, UV_TIMER, sched, k_setuptimer, returntrue, NULL);
	} else {
		return lcuT_resetcohdlk(L, UV_IDLE, sched, k_setupidle, returntrue, NULL);
	}
	return returntrue(L);
}


LCUI_FUNC void lcuM_addtimef (lua_State *L) {
	static const luaL_Reg luaf[] = {
		{"nanosecs", system_nanosecs},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"time", system_time},
		{"suspend", system_suspend},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, luaf, 0);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
