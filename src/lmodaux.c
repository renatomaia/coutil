#include "lmodaux.h"

#include <lualib.h>


LCUI_FUNC int lcuL_pusherrres (lua_State *L, int err) {
	lua_pushnil(L);
	lcu_pusherror(L, err);
	lua_pushinteger(L, -err);
	return 3;
}

LCUI_FUNC int lcuL_pushresults (lua_State *L, int n, int err) {
	if (err < 0) {
		lua_pop(L, n);
		return lcuL_pusherrres(L, err);
	} else if (n == 0) {
		lua_pushboolean(L, 1);
		return 1;
	}
	return n;
}


static const luaL_Reg stdlibs[] = {
	{"_G", luaopen_base},
	{LUA_COLIBNAME, luaopen_coroutine},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_IOLIBNAME, luaopen_io},
	{LUA_OSLIBNAME, luaopen_os},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_MATHLIBNAME, luaopen_math},
	{LUA_UTF8LIBNAME, luaopen_utf8},
	{LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
	{LUA_BITLIBNAME, luaopen_bit32},
#endif
	{NULL, NULL}
};

static int writer (lua_State *L, const void *b, size_t size, void *B) {
	(void)L;
	luaL_addlstring((luaL_Buffer *) B, (const char *)b, size);
	return 0;
}

LCUI_FUNC lua_State *lcuL_newstate (lua_State *L) {
	const luaL_Reg *lib;
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	lua_CFunction panic = lua_atpanic(L, NULL);  /* changes panic function */
	lua_State *NL = lua_newstate(allocf, allocud);

	lua_atpanic(L, panic);  /* restore panic function */
	lua_atpanic(NL, panic);

	luaL_checkstack(NL, 3, "not enough memory");
	luaL_requiref(NL, LUA_LOADLIBNAME, luaopen_package, 0);
	luaL_getsubtable(NL, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);

	/* add standard libraries to 'package.preload' */
	for (lib = stdlibs; lib->func; lib++) {
		lua_pushcfunction(NL, lib->func);
		lua_setfield(NL, -2, lib->name);
	}

	/* copy 'package.preload' */
	lua_pushnil(L);  /* first key */
	while (lua_next(L, -2) != 0) {
		if (lua_isstring(L, -2) && lua_isfunction(L, -1)) {
			size_t len;
			const char *name = lua_tolstring(L, -2, &len);
			lua_pushlstring(NL, name, len);
			if (lua_iscfunction(L, -1)) {
				lua_CFunction loader = lua_tocfunction(L, -1);
				lua_pushcfunction(NL, loader);
			} else {
				luaL_Buffer b;
				int err;
				luaL_buffinit(NL, &b);
				err = lua_dump(L, writer, &b, 0);
				luaL_pushresult(&b);
				if (err) {
					lua_pop(NL, 1);
					lua_pushnil(NL);
				} else {
					size_t l;
					const char *bytecodes = lua_tolstring(NL, -1, &l);
					int status = luaL_loadbufferx(NL, bytecodes, l, NULL, "b");
					lcu_assert(status == LUA_OK);
					lua_remove(NL, -2);
				}
			}
			lua_settable(NL, -3);
		}
		lua_pop(L, 1);
	}
	lua_pop(NL, 2);  /* remove 'package' and 'LUA_PRELOAD_TABLE' */
	lua_pop(L, 1);  /* remove 'LUA_PRELOAD_TABLE' */
	return NL;
}

LCUI_FUNC int lcuL_canmove (lua_State *L,
                            int n,
                            lcuL_CustomTransfer customf) {
	int i, top = lua_gettop(L);
	for (i = 1+top-n; i <= top; ++i) {
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TNIL:
			case LUA_TBOOLEAN:
			case LUA_TNUMBER:
			case LUA_TSTRING: break;
			default: {
				if (customf == NULL || !customf(NULL, L, i, type)) {
					const char *tname = luaL_typename(L, i);
					lua_settop(L, 0);
					lua_pushnil(L);
					lua_pushfstring(L, "unable to transfer argument #%d (got %s)", i, tname);
					return 0;
				}
			}
		}
	}
	return 1;
}

static void pushfrom (lua_State *to,
                      lua_State *from,
                      int idx,
                      lcuL_CustomTransfer customf) {
	int type = lua_type(from, idx);
	switch (type) {
		case LUA_TNIL: {
			lua_pushnil(to);
		} break;
		case LUA_TBOOLEAN: {
			lua_pushboolean(to, lua_toboolean(from, idx));
		} break;
		case LUA_TNUMBER: {
			lua_pushnumber(to, lua_tonumber(from, idx));
		} break;
		case LUA_TSTRING: {
			size_t l;
			const char *s = lua_tolstring(from, idx, &l);
			lua_pushlstring(to, s, l);
		} break;
		default: {
			if (customf && customf(to, from, idx, type)) break;
			else {
				const char *tname = luaL_typename(from, idx);
				luaL_error(to, "unable to transfer argument #%d (got %s)", idx, tname);
			}
		}
	}
}

static int auxpushfrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int idx = lua_tointeger(to, 2);
	lcuL_CustomTransfer customf = (lcuL_CustomTransfer)lua_touserdata(to, 4);
	pushfrom(to, from, idx, customf);
	return 1;
}

LCUI_FUNC int lcuL_pushfrom (lua_State *to,
                             lua_State *from,
                             int idx,
                             lcuL_CustomTransfer customf) {
	lcu_assert(lua_gettop(from) >= idx);
	if (!lua_checkstack(to, 4)) return LUA_ERRMEM;
	lua_pushcfunction(to, auxpushfrom);
	lua_pushlightuserdata(to, from);
	lua_pushinteger(to, idx);
	lua_pushlightuserdata(to, customf);
	return lua_pcall(to, 3, 1, 0);
}

static int auxmovefrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int n = lua_tointeger(to, 2);
	lcuL_CustomTransfer customf = (lcuL_CustomTransfer)lua_touserdata(to, 3);
	int top = lua_gettop(from);
	int idx;
	lua_settop(to, 0);
	luaL_checkstack(to, n, "too many values");
	for (idx = 1+top-n; idx <= top; idx++) pushfrom(to, from, idx, customf);
	return n;
}

LCUI_FUNC int lcuL_movefrom (lua_State *to,
                             lua_State *from,
                             int n,
                             lcuL_CustomTransfer customf) {
	int status;
	lcu_assert(lua_gettop(from) >= n);
	if (!lua_checkstack(to, 4)) return LUA_ERRMEM;
	lua_pushcfunction(to, auxmovefrom);
	lua_pushlightuserdata(to, from);
	lua_pushinteger(to, n);
	lua_pushlightuserdata(to, customf);
	status = lua_pcall(to, 3, n, 0);
	if (status == LUA_OK) lua_pop(from, n);
	return status;
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

#define LCU_UVLOOPCLS	LCU_PREFIX"uv_loop_t"

static void closehandle (uv_handle_t* handle, void* arg) {
	if (!uv_is_closing(handle)) uv_close(handle, NULL);
}

static int terminateloop (lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)luaL_checkudata(L, 1, LCU_UVLOOPCLS);
	int err = uv_loop_close(loop);
	if (err == UV_EBUSY) {
		uv_walk(loop, closehandle, NULL);
		loop->data = (void *)L;
		err = uv_run(loop, UV_RUN_DEFAULT);
		loop->data = NULL;
		if (!err) err = uv_loop_close(loop);

else {fprintf(stderr, "WARN: close failed!\n"); uv_print_all_handles(loop, stderr);}

	}
	lcu_assert(!err);
	return 0;
}

LCUI_FUNC void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv) {
	int err;
	lua_newtable(L);  /* LCU_COREGISTRY */
	pushhandlemap(L);  /* LCU_HANDLEMAP */
	if (uv) lua_pushlightuserdata(L, uv);
	else {
		uv = (uv_loop_t *)lua_newuserdata(L, sizeof(uv_loop_t));
		if (luaL_newmetatable(L, LCU_UVLOOPCLS)) {
			lua_pushvalue(L, -4);
			lua_pushvalue(L, -4);
			lua_pushcclosure(L, terminateloop, 2);
			lua_setfield(L, -2, "__gc");
		}
		lua_setmetatable(L, -2);
	}
	err = uv_loop_init(uv);
	if (err < 0) lcu_error(L, err);
	uv->data = NULL;
}

LCUI_FUNC void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -(nup+1));
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -2, l->name);
	}
}

LCUI_FUNC void lcuM_newclass (lua_State *L, const char *name) {
	luaL_newmetatable(L, name);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
}


LCUI_FUNC void lcuL_printstack (lua_State *L, const char *file, int line,
                                              const char *func) {
	int i;
	printf("%s:%d: function '%s'\n", file, line, func);
	for(i = 1; i <= lua_gettop(L); ++i) {
		const char *typename = NULL;
		printf("\t[%d] = ", i);
		switch (lua_type(L, i)) {
			case LUA_TNUMBER:
				printf("%g", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("\"%s\"", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf(lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNIL:
				printf("nil");
				break;
			case LUA_TUSERDATA:
				if (lua_getmetatable(L, i)) {
					lua_getfield(L, -1, "__name");
					typename = lua_tostring(L, -1);
					lua_pop(L, 2);
				}
			default:
				printf("%s: %p", typename ? typename : luaL_typename(L, i),
				                 lua_topointer(L, i));
				break;
		}
		printf("\n");
	}
	printf("\n");
}
