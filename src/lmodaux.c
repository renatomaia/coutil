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

LCUI_FUNC void lcuL_warnerror (lua_State *L, const char *msg, int err) {
	lua_warning(L, LCU_WARNPREFIX, 1);
	lua_warning(L, msg, 1);
	lua_warning(L, uv_strerror(err), 0);
}

LCUI_FUNC void lcuL_setfinalizer (lua_State *L,
                                  lua_CFunction finalizer,
                                  int nup) {
	lua_pushcclosure(L, finalizer, nup);
	lua_createtable(L, 0, 1);
	lua_insert(L, -2);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
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

	/* copy channel map reference */
	lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
	lcu_assert(lua_touserdata(L, -1) != NULL);
	lua_pushlightuserdata(NL, lua_touserdata(L, -1));
	lua_setfield(NL, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
	lua_pop(L, 1);

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

#define doerrmsg(F,L,I,M,T) (I > 0 ? \
	F(L, "unable to transfer %s #%d (got %s)", M, I, T) : \
	F(L, "unable to transfer %s (got %s)", M, T))

LCUI_FUNC int lcuL_canmove (lua_State *L,
                            int n,
                            const char *msg) {
	int i, top = lua_gettop(L);
	for (i = 1+top-n; i <= top; ++i) {
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TNIL:
			case LUA_TBOOLEAN:
			case LUA_TNUMBER:
			case LUA_TSTRING:
			case LUA_TLIGHTUSERDATA:
				break;
			default: {
				const char *tname = luaL_typename(L, i);
				lua_settop(L, 0);
				lua_pushnil(L);
				doerrmsg(lua_pushfstring, L, i, msg, tname);
				return 0;
			}
		}
	}
	return 1;
}

static void pushfrom (lua_State *to,
                      lua_State *from,
                      int idx,
                      const char *msg) {
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
		case LUA_TLIGHTUSERDATA: {
			lua_pushlightuserdata(to, lua_touserdata(from, idx));
		} break;
		default: {
			const char *tname = luaL_typename(from, idx);
			doerrmsg(luaL_error, to, idx, msg, tname);
		}
	}
}

static int auxpushfrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int idx = lua_tointeger(to, 2);
	const char *msg = lua_tostring(to, 3);
	pushfrom(to, from, idx, msg);
	return 1;
}

LCUI_FUNC int lcuL_pushfrom (lua_State *to,
                             lua_State *from,
                             int idx,
                             const char *msg) {
	lcu_assert(lua_gettop(from) >= idx);
	if (!lua_checkstack(to, 4)) return LUA_ERRMEM;
	lua_pushcfunction(to, auxpushfrom);
	lua_pushlightuserdata(to, from);
	lua_pushinteger(to, idx);
	lua_pushstring(to, msg);
	return lua_pcall(to, 3, 1, 0);
}

static int auxmovefrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int n = lua_tointeger(to, 2);
	const char *msg = lua_tostring(to, 3);
	int top = lua_gettop(from);
	int idx;
	lua_settop(to, 0);
	luaL_checkstack(to, n, "too many values");
	for (idx = 1+top-n; idx <= top; idx++) pushfrom(to, from, idx, msg);
	return n;
}

LCUI_FUNC int lcuL_movefrom (lua_State *to,
                             lua_State *from,
                             int n,
                             const char *msg) {
	int status;
	lcu_assert(lua_gettop(from) >= n);
	if (!lua_checkstack(to, 4)) return LUA_ERRMEM;
	lua_pushcfunction(to, auxmovefrom);
	lua_pushlightuserdata(to, from);
	lua_pushinteger(to, n);
	lua_pushstring(to, msg);
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

static void closehandle (uv_handle_t* handle, void* arg) {
	if (!uv_is_closing(handle)) uv_close(handle, NULL);
}

static int terminateloop (lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserdata(L, 1);
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
	{
		lcu_ActiveOps *ops =
			(lcu_ActiveOps *)lua_newuserdatauv(L, sizeof(lcu_ActiveOps), 0);
		ops->asyncs = 0;
		ops->others = 0;
	}
	if (uv) lua_pushlightuserdata(L, uv);
	else {
		int i;
		uv = (uv_loop_t *)lua_newuserdatauv(L, sizeof(uv_loop_t), 0);
		for (i = 0; i < LCU_MODUPVS; ++i) lua_pushvalue(L, -5);
		lcuL_setfinalizer(L, terminateloop, LCU_MODUPVS);
		err = uv_loop_init(uv);
		if (err < 0) lcu_error(L, err);
	}
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
