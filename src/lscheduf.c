#include "lmodaux.h"
#include "loperaux.h"


static int k_resumeall (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = (uv_loop_t *)kctx;
	int pending;
	lcu_assert(status == LUA_YIELD);
	lua_settop(L, 0);
	lcu_log(loop, L, "resuming threads (yieldable)");
	pending = uv_run(loop, UV_RUN_DEFAULT);
	if (pending && lua_isuserdata(L, 1)) {
		lcu_log(loop, L, "suspending resuming threads");
		return lua_yieldk(L, 1, kctx, k_resumeall);
	}
	lcu_log(loop, L, "done resuming threads (yieldable)");
	loop->data = NULL;
	lua_pushboolean(L, pending);
	return 1;
}

static int lcuM_run (lua_State *L) {
	static const char *const opts[] = {"loop", "step", "ready", NULL};
	lcu_Scheduler *sched = lcu_getsched(L);
	uv_loop_t *loop = lcu_toloop(sched);
	int pending;
	uv_run_mode mode = luaL_checkoption(L, 1, "loop", opts);
	if (loop->data != NULL) luaL_error(L, "already running");
	lua_settop(L, 0);
	if (mode == UV_RUN_DEFAULT && lua_isyieldable(L)) {
		int ltype = lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
		lua_pop(L, 1);
		if (ltype == LUA_TUSERDATA) {
			lua_State *mainL;
			lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
			mainL = lua_tothread(L, -1);
			lua_pop(L, 1);
			if (L == lua_tothread(mainL, 1)) {
				lua_KContext kctx = (lua_KContext)loop;
				loop->data = (void *)L;
				if (lcu_shallsuspend(sched)) {
					lcu_log(loop, L, "suspending resuming threads");
					lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
					return lua_yieldk(L, 1, kctx, k_resumeall);
				}
				return k_resumeall(L, LUA_YIELD, kctx);
			}
		}
	}
	loop->data = (void *)L;
	lcu_log(loop, L, "resuming threads");
	pending = uv_run(loop, mode);
	lcu_log(loop, L, "done resuming threads");
	loop->data = NULL;
	lua_pushboolean(L, pending);
	return 1;
}

static int lcuM_isrunning (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	uv_loop_t *loop = lcu_toloop(sched);
	lua_pushboolean(L, loop->data != NULL);
	return 1;
}

static int lcuM_halt (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	uv_loop_t *loop = lcu_toloop(sched);
	if (!loop->data) luaL_error(L, "not running");
	uv_stop(loop);
	return 0;
}

static int lcuM_printall (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	uv_loop_t *loop = lcu_toloop(sched);
	uv_print_all_handles(loop, stderr);
	return 0;
}

LCUI_FUNC void lcuM_addscheduf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"run", lcuM_run},
		{"isrunning", lcuM_isrunning},
		{"halt", lcuM_halt},
		{"printall", lcuM_printall},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
