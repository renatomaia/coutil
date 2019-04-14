#include "lmodaux.h"
#include "looplib.h"


LOOPLIB_API void loop_setmethods (lua_State *L, const luaL_Reg *meth, int nup) {
	lua_pushvalue(L, -(nup+1));
	lua_setfield(L, -(nup+2), "__index");
	luaL_setfuncs(L, meth, nup);
}

LOOPLIB_API void loop_makesubclass (lua_State *L, int superidx) {
	static const char *const metameth[] = {
		"__add",
		"__sub",
		"__mul",
		"__div",
		"__mod",
		"__pow",
		"__unm",
		"__idiv",
		"__band",
		"__bor",
		"__bxor",
		"__bnot",
		"__shl",
		"__shr",
		"__concat",
		"__len",
		"__eq",
		"__lt",
		"__le",
		"__index",
		"__newindex",
		"__call",
		"__mode",
		"__gc",
		"__tostring",
		"__pairs",
		NULL
	};
	int i;
	lua_pushvalue(L, superidx);
	for (i = 0; metameth[i]; ++i) {
		if (lua_getfield(L, -2, metameth[i]) == LUA_TNIL) {  /* not defined? */
			lua_getfield(L, -2, metameth[i]);  /* get metamethod from superclass */
			lua_setfield(L, -4, metameth[i]);  /* set metamethod of subclass */
		}
		lua_pop(L, 1);  /* remove metamethod */
	}
	lua_setmetatable(L, -2);
}

LOOPLIB_API int loop_issubclass (lua_State *L, int idx) {
	int found = 0;
	lua_pushvalue(L, idx);
	while (!(found = lua_rawequal(L, -1, -2)) && lua_getmetatable(L, -1)){
		lua_remove(L, -2);  /* remove previous metatable */
	}
	lua_pop(L, 1);  /* remove last metatable */
	return found;
}

LOOPLIB_API void loopL_newclass (lua_State *L, const char *name,
                                               const char *super) {
	luaL_newmetatable(L, name);  /* create metatable for instances */
	if (super) {
		luaL_getmetatable(L, super);
		lua_insert(L, -2);  /* place super-metatable below metatable */
		loop_makesubclass(L, -2);
		lua_remove(L, -2);  /* remove super-metatable */
	}
}

LOOPLIB_API void *loopL_testinstance (lua_State *L, int idx, const char *cls) {
	void *p = lua_touserdata(L, idx);
	if (p && lua_getmetatable(L, idx)) {  /* get userdata's metatable */
		luaL_getmetatable(L, cls);  /* get class */
		if (!loop_issubclass(L, -2)) p = NULL;
		lua_pop(L, 2);  /* remove metatable and class */
		return p;
	}
	return NULL;  /* value is not a userdata with a metatable */
}

LOOPLIB_API void *loopL_checkinstance (lua_State *L, int idx, const char *cls) {
	void *p = loopL_testinstance(L, idx, cls);
	if (p == NULL) {
		const char *msg = lua_pushfstring(L, "%s expected, got %s",
		                                  cls, luaL_typename(L, idx));
		luaL_argerror(L, idx, msg);
		return NULL;
	}
	return p;
}
