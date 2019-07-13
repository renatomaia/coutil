#include "lmodaux.h"


static int lcuM_run (lua_State *L) {
	static const char *const opts[] = {"loop", "step", "ready", NULL};
	uv_loop_t *loop = lcu_toloop(L);
	int pending;
	int mode = luaL_checkoption(L, 1, "loop", opts);
	if (loop->data) luaL_error(L, "already running");
	lua_settop(L, 2);  /* set trap function as top */
	loop->data = (void *)L;
	pending = uv_run(loop, mode);
	loop->data = NULL;
	lua_pushboolean(L, pending);
	return 1;
}

static int lcuM_isrunning (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	lua_pushboolean(L, loop->data != NULL);
	return 1;
}

static int lcuM_halt (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	if (!loop->data) luaL_error(L, "not running");
	uv_stop(loop);
	return 0;
}

static int lcuM_printall (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	uv_print_all_handles(loop, stderr);
	return 0;
}

LCULIB_API void lcuM_addscheduf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"run", lcuM_run},
		{"isrunning", lcuM_isrunning},
		{"halt", lcuM_halt},
		{"printall", lcuM_printall},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
