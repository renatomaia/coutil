#include "lmodaux.h"


static int lcuM_run (lua_State *L) {
	static const char *const opts[] = {"loop", "step", "ready", NULL};
	uv_loop_t *loop = lcu_toloop(L);
	int pending;
	int mode = luaL_checkoption(L, 1, "loop", opts);
	luaL_argcheck(L, !loop->data, 1, "already running");
	lua_settop(L, 2);  /* set trap function as top */
	loop->data = (void *)L;
	pending = uv_run(loop, mode);
	loop->data = NULL;
	lua_pushboolean(L, pending);
	return 1;
}

LCULIB_API void lcuM_addscheduf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"run", lcuM_run},
		{NULL, NULL}
	};
	lcuM_addmodfunc(L, modf);
}
