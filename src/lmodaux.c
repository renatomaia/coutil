#include "lmodaux.h"


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
		err = uv_run(loop, UV_RUN_NOWAIT);
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
