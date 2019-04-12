#include "lmodaux.h"


#if !defined(lcu_assert)
#define lcu_assert(X)	((void)(X))
#endif


#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

LCULIB_API void lcu_checkerr (lua_State *L, int err) {
	if (err < 0) lcu_error(L, err);
}


#define lcu_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(1))
#define LCU_MODUPVS	3


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

LCULIB_API void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv) {
	if (uv) lua_pushlightuserdata(L, uv);
	else uv = (uv_loop_t *)lua_newuserdata(L, sizeof(uv_loop_t));
	lcu_checkerr(L, uv_loop_init(uv));
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
