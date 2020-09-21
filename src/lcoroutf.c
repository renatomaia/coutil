#include "lsyslib.h"
#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>


static int doloaded (lua_State *L, lua_State *NL, int status) {
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		size_t len;
		const char *errmsg = lua_tolstring(NL, -1, &len);
		lua_pushnil(L);
		lua_pushlstring(L, errmsg, len);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	lcu_newsysco(L, NL);
	return 1;
}

/* succ [, errmsg] = system.load(chunk, chunkname, mode) */
static int system_load (lua_State *L) {
	size_t l;
	const char *s = luamem_checkstring(L, 1, &l);
	const char *chunkname = luaL_optstring(L, 2, s);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	return doloaded(L, NL, status);
}

/* succ [, errmsg] = system.loadfile(filepath, mode) */
static int system_loadfile (lua_State *L) {
	const char *fpath = luaL_optstring(L, 1, NULL);
	const char *mode = luaL_optstring(L, 2, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return doloaded(L, NL, status);
}


/* getmetatable(coroutine).__gc(coroutine) */
static int coroutine_gc(lua_State *L) {
	lcuT_closesysco(L, 1);
	return 0;
}


/* succ = object:close() */
static int coroutine_close(lua_State *L) {
	int closed = lcuT_closesysco(L, 1);
	lua_pushboolean(L, closed);
	return 1;
}


/* status = coroutine:status() */
static int coroutine_status(lua_State *L) {
	lcu_SysCoro *sysco = lcu_checksysco(L, 1);
	if (lcu_issyscoclosed(sysco)) lua_pushliteral(L, "dead");
	else if (lcu_tosyscoparent(sysco)) lua_pushliteral(L, "running");
	else {
		lua_State *co = lcu_tosyscolua(sysco);
		if (lua_status(co) == LUA_YIELD) lua_pushliteral(L, "suspended");
		else if (lua_status(co) != LUA_OK) lua_pushliteral(L, "dead");
		else if (lua_gettop(co)) lua_pushliteral(L, "normal");
		else lua_pushliteral(L, "dead");
	}
	return 1;
}


/* succ [, errmsg] = system.resume(coroutine) */
static int returnvalues (lua_State *L) {
	return lua_gettop(L)-1;  /* return all except the coroutine (arg #1) */
}
static void uv_onworking(uv_work_t* req) {
	lcu_SysCoro *sysco = (lcu_SysCoro *)req->data;
	lua_State *co = lcu_tosyscolua(sysco);
	int narg = lua_gettop(co);
	int status;
	if (lua_status(co) == LUA_OK) --narg;  /* function on stack */
	status = lua_resume(co, NULL, narg, &narg);
	lcu_assert(lua_checkstack(co, 2));
	lua_pushinteger(co, narg);
	lua_pushinteger(co, status);
}
static void uv_onworked(uv_work_t* work, int status) {
	uv_loop_t *loop = work->loop;
	uv_req_t *request = (uv_req_t *)work;
	lcu_SysCoro *sysco = (lcu_SysCoro *)request->data;
	lua_State *co = lcu_tosyscolua(sysco);
	lua_State *thread;
	lua_State *L = (lua_State *)loop->data;
	request->data = lcu_tosyscoparent(sysco);  /* restore 'lua_State' on conclusion */
	thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret;
		if (status == UV_ECANCELED) {
			lua_settop(co, lua_status(co) == LUA_OK);  /* keep function on stack */
			lua_pushboolean(thread, 0);
			lua_pushliteral(thread, "cancelled");
			nret = 2;
		} else {
			int lstatus = (int)lua_tointeger(co, -1);
			nret = (int)lua_tointeger(co, -2);
			lua_pop(co, 2);  /* remove lstatus and nret values */
			if (lstatus == LUA_OK || lstatus == LUA_YIELD) {
				lua_pushboolean(thread, 1);  /* return 'true' to signal success */
				if (lcuL_movefrom(thread, co, nret, "return value") != LUA_OK) {
					lua_pop(co, nret);  /* remove results anyway */
					lua_pushboolean(thread, 0);
					lua_replace(thread, -3);  /* remove pushed 'true' that signals success */
					nret = 2;
				}
			} else {
				lua_pushboolean(thread, 0);
				if (lcuL_pushfrom(thread, co, -1, "error") != LUA_OK) {
					lua_pop(co, 1);  /* remove error anyway */
				}
				nret = 2;
			}
		}
		lcuT_stopsysco(L, sysco);  /* frees 'co' if closed */
		lcuU_resumereqop(loop, request, nret);
	}
	else lcuT_stopsysco(L, sysco);  /* frees 'co' if closed */
}
static int k_setupwork (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_work_t *work = (uv_work_t *)request;
	lcu_SysCoro *sysco = lcu_checksysco(L, 1);
	lua_State *co = lcu_tosyscolua(sysco);
	int narg = lua_gettop(L)-1;
	int err;
	lcu_assert(request->data == L);
	if (lcu_tosyscoparent(sysco)) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "cannot resume running coroutine");
		return 2;
	}
	if ( lcu_issyscoclosed(sysco) ||
	     (lua_status(co) == LUA_OK && lua_gettop(co) == 0) ) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "cannot resume dead coroutine");
		return 2;
	}
	if (lcuL_movefrom(co, L, narg, "argument") != LUA_OK) {
		const char *msg = lua_tostring(co, -1);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, msg);
		lua_pop(co, 1);
		return 2;
	}
	request->data = sysco;  /* CAUTION: Only moment when 'data' is not a */
	                        /*          'lua_State'. Only while it is running */
	err = uv_queue_work(loop, work, uv_onworking, uv_onworked);
	if (err < 0) {
		request->data = L;  /* restore 'lua_State' on failure */
		lua_pop(co, narg);  /* restore coroutine stack */
		return lcuL_pusherrres(L, err);
	}
	lcuT_startsysco(L, sysco);
	return -1;  /* yield on success */
}
static int system_resume (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetreqopk(L, sched, k_setupwork, returnvalues, NULL);
}

LCUI_FUNC void lcuM_addcoroutc (lua_State *L) {
	static const luaL_Reg clsf[] = {
		{"__gc", coroutine_gc},
		{"close", coroutine_close},
		{"status", coroutine_status},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_SYSCOROCLS);
	lcuM_setfuncs(L, clsf, 0);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addcoroutf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"load", system_load},
		{"loadfile", system_loadfile},
		{NULL, NULL}
	};
	static const luaL_Reg upvf[] = {
		{"resume", system_resume},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
	lcuM_setfuncs(L, upvf, LCU_MODUPVS);
}
