#include "lauxlib.h"
#include "lmodaux.h"


/* succ [, errmsg] = coroutine.load(chunk, chunkname, mode) */
static int k_yieldsaved (lua_State *L, int status, lua_KContext kctx) {
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!kctx);
	if (lua_touserdata(L, 1) != k_yieldsaved) luaL_error(L, "stack corrupted");
	return lua_gettop(L)-1;
}
static int test_yieldsaved (lua_State *L) {
	int narg = lua_gettop(L);
	lua_pushlightuserdata(L, k_yieldsaved);
	lua_insert(L, 1);
	return lua_yieldk(L, narg, (lua_KContext)0, k_yieldsaved);
}


LCUMOD_API int luaopen_coutil_test (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"yieldsaved", test_yieldsaved},
		{NULL, NULL}
	};
	luaL_newlib(L, modf);
	return 1;
}
