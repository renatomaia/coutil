#include "lmodaux.h"


LCULIB_API void lcu_chkerror (lua_State *L, int err) {
	if (err < 0) lcu_error(L, err);
}


static void pushhandlemap (lua_State *L) {
	lua_pushlightuserdata(L, pushhandlemap);
	if (lua_gettable(L, LUA_REGISTRYINDEX) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushlightuserdata(L, pushhandlemap);
		lua_pushvalue(L, -2);
		lua_createtable(L, 0, 1);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
}

#define LCU_UVLOOPCLS	LCU_PREFIX"EventLoop"

static int terminateloop (lua_State *L) {
	uv_loop_t *uv = (uv_loop_t *)luaL_checkudata(L, 1, LCU_UVLOOPCLS);
	int err = uv_loop_close(uv);
	if (err == UV_EBUSY && uv_run(uv, UV_RUN_NOWAIT) == 0)
		err = uv_loop_close(uv);
	lcu_assert(!err);
	return 0;
}

LCULIB_API void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv) {
	if (uv) lua_pushlightuserdata(L, uv);
	else {
		uv = (uv_loop_t *)lua_newuserdata(L, sizeof(uv_loop_t));
		if (luaL_newmetatable(L, LCU_UVLOOPCLS)) {
			lua_pushcfunction(L, terminateloop);
			lua_setfield(L, -2, "__gc");
		}
		lua_setmetatable(L, -2);
	}
	lcu_chkerror(L, uv_loop_init(uv));
	lua_newtable(L);  /* LCU_COREGISTRY */
	pushhandlemap(L);  /* LCU_HANDLEMAP */
}

LCULIB_API void lcuM_addmodfunc (lua_State *L, const luaL_Reg *l) {
	for (; l->name; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < LCU_MODUPVS; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -(LCU_MODUPVS+1));
		lua_pushcclosure(L, l->func, LCU_MODUPVS);  /* closure with upvalues */
		lua_setfield(L, -2, l->name);
	}
}
