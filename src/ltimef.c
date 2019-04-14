#include "lmodaux.h"
#include "loperaux.h"


static void lcuB_oncallback (uv_handle_t *handle) {
	lua_State *co = (lua_State *)handle->data;
	lcu_assert(co != NULL);
	lcu_PendingOp *op = (lcu_PendingOp *)handle;
	lua_pushboolean(co, 1);
	lcu_resumeop(op, co);
}

static void lcuB_onidle (uv_idle_t *h) {
	lcuB_oncallback((uv_handle_t *)h);
}

static int lcuK_setupidle (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	uv_handle_t *handle = lcu_tohandle(op);
	uv_idle_t *idle = (uv_idle_t *)handle;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	lua_settop(L, 0);  /* discard yield results */
	lcu_chkinitop(L, op, loop, uv_idle_init(loop, idle));
	lcu_chkstarthdl(L, handle, uv_idle_start(idle, lcuB_onidle));
	return lcu_yieldop(L, 0, lcuK_chkignoreop, op);
}

static void lcuB_ontimer (uv_timer_t *h) {
	lcuB_oncallback((uv_handle_t *)h);
}

static int lcuK_setuptimer (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	uv_handle_t *handle = lcu_tohandle(op);
	uv_timer_t *timer = (uv_timer_t *)handle;
	uint64_t msecs = (uint64_t)(ctx);
	lcu_assert(status == LUA_YIELD);
	lua_settop(L, 0);  /* discard yield results */
	lcu_chkinitop(L, op, loop, uv_timer_init(loop, timer));
	lcu_chkstarthdl(L, handle, uv_timer_start(timer, lcuB_ontimer, msecs, msecs));
	return lcu_yieldop(L, 0, lcuK_chkignoreop, op);
}

/* succ [, errmsg, ...] = system.pause([delay]) */
static int lcuM_pause (lua_State *L) {
	lcu_PendingOp *op;
	lua_Number delay = luaL_optnumber(L, 1, 0);
	lua_settop(L, 0);  /* discard all arguments */
	if (delay > 0) {
		uv_handle_t *handle;
		uv_timer_t *timer;
		uint64_t msecs = (uint64_t)(delay*1000);
		op = lcu_resethdl(L, UV_TIMER, (lua_KContext)msecs, lcuK_setuptimer);
		handle = lcu_tohandle(op);
		timer = (uv_timer_t *)handle;
		if (uv_timer_get_repeat(timer) != msecs) {
			lcu_chkerror(L, uv_timer_stop(timer));
			lcu_chkstarthdl(L, handle, uv_timer_start(timer, lcuB_ontimer, msecs,
			                                                               msecs));
		}
	}
	else op = lcu_resethdl(L, UV_IDLE, 0, lcuK_setupidle);
	return lcu_yieldop(L, 0, lcuK_chkignoreop, op);
}


LCULIB_API void lcuM_addtimef (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"pause", lcuM_pause},
		{NULL, NULL}
	};
	lcuM_addmodfunc(L, modf);
}
