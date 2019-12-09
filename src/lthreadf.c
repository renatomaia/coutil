#include <lmemlib.h>

#include "lmodaux.h"


/* succ [, errmsg] = system.thread(chunk, chunkname, mode, ...) */
static void resumethread(uv_work_t* req) {
	lua_State *L = (lua_State *)req->data;
	int status = lua_resume(L, NULL, 0);
	if (status == LUA_YIELD) {
		lua_settop(L, 1);  /* keep 'uv_work_t' on stack */
	} else {
		req->data = NULL;
		lua_close(L);
	}
}
static void onthreadyield(uv_work_t* req, int status) {
	lua_State *L = (lua_State *)req->data;
	if (L) {
		if (status == 0) {
			uv_queue_work(req->loop, req, resumethread, onthreadyield);
		} else {
			req->data = NULL;
			lua_close(L);
		}
	}
}
static int system_thread (lua_State *L) {
	size_t l;
	const char *s = luamem_checkstring(L, 1, &l);
	const char *chunkname = luaL_optstring(L, 2, s);
	const char *mode = luaL_optstring(L, 3, "bt");
	lua_State *NL = luaL_newstate();  /* create state */
	uv_work_t* req = (uv_work_t*)lua_newuserdata(NL, sizeof(uv_work_t));
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		const char *err = lua_tostring(NL, -1);
		lua_pushnil(L);
		lua_pushstring(L, err);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	} else {
		uv_loop_t *loop = lcu_toloop(L);
		req->data = NL;
		uv_queue_work(loop, req, resumethread, onthreadyield);
		lua_pushboolean(L, 1);
		return 1;
	}
}


LCUI_FUNC void lcuM_addthreadf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"thread", system_thread},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
