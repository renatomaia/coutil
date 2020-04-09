#include "lsyslib.h"
#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>


static int system_coroutine (lua_State *L) {
	size_t l;
	const char *s = luamem_checkstring(L, 1, &l);
	const char *chunkname = luaL_optstring(L, 2, s);
	const char *mode = luaL_optstring(L, 3, "bt");
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	lua_State *NL = lua_newstate(allocf, allocud);  /* create state */
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		const char *errmsg = lua_tostring(NL, -1);
		lua_pushnil(L);
		lua_pushstring(L, errmsg);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	lcu_newsysco(L, NL);
	return 1;
}


/* getmetatable(coroutine).__gc(coroutine) */
static int coroutine_gc(lua_State *L) {
	lcu_closesysco(L, 1);
	return 0;
}


/* succ = object:close() */
static int coroutine_close(lua_State *L) {
	int closed = lcu_closesysco(L, 1);
	lua_pushboolean(L, closed);
	return 1;
}


/* status = coroutine:status() */
static int coroutine_status(lua_State *L) {
	lcu_SysCoro *sysco = lcu_checksysco(L, 1);
	if (lcu_issyscoclosed(sysco)) lua_pushliteral(L, "dead");
	else if (lcu_issyscorunning(sysco)) lua_pushliteral(L, "running");
	else {
		lua_State *co = lcu_tosyscolua(sysco);
		if (lua_status(co) == LUA_YIELD) lua_pushliteral(L, "suspended");
		else if (lua_status(co) != LUA_OK) lua_pushliteral(L, "dead");
		else if (lua_gettop(co)) lua_pushliteral(L, "suspended");
		else lua_pushliteral(L, "dead");
	}
	return 1;
}


 /* succ [, errmsg] = system.thread(chunk, chunkname, mode, ...) */
static int movevals (lua_State *from, lua_State *to, int n) {
	int i;
	lcu_assert(lua_gettop(from) < n);
	luaL_checkstack(to, n, "too many arguments to resume");
	for (i = 0; i < n; i++) {
		switch (lua_type(from, i-n)) {
			case LUA_TNIL: {
				lua_pushnil(to);
			} break;
			case LUA_TBOOLEAN: {
				lua_pushboolean(to, lua_toboolean(from, i-n));
			} break;
			case LUA_TNUMBER: {
				lua_pushnumber(to, lua_tonumber(from, i-n));
			} break;
			case LUA_TSTRING: {
				size_t l;
				const char *s = luamem_tostring(from, i-n, &l);
				lua_pushlstring(to, s, l);
			} break;
			default:
				lua_pop(to, i);
				return lua_gettop(from)+1+i-n;
		}
	}
	lua_pop(from, n);
	return 0;
}
static int returnvalues (lua_State *L) {
	return lua_gettop(L)-1;  /* return all except the coroutine (arg #1) */
}
static void uv_onworking(uv_work_t* req) {
	lcu_SysCoro *sysco = (lcu_SysCoro *)req->data;
	lua_State *co = lcu_tosyscolua(sysco);
	int narg = lua_gettop(co);
	if (lua_status(co) == LUA_OK) --narg;  /* function on stack */
	int status = lua_resume(co, NULL, narg);
	lua_pushinteger(co, status);
}
static void uv_onworked(uv_work_t* work, int status) {
	uv_loop_t *loop = work->loop;
	uv_req_t *request = (uv_req_t *)work;
	lcu_SysCoro *sysco = (lcu_SysCoro *)request->data;
	lua_State *co = lcu_tosyscolua(sysco);
	lua_State *thread;
	request->data = lcu_tosyscoparent(sysco);  /* restore 'lua_State' on conclusion */
	thread = lcuU_endreqop(loop, request);
	if (thread) {
		int lstatus = (int)lua_tointeger(co, -1);
		lcu_assert(lua_gettop(thread) == 1);  /* only the coroutine on stack */
		lua_pop(co, 1);  /* remove lstatus value */
		if (lstatus == LUA_OK || lstatus == LUA_YIELD) {
			int nres = lua_gettop(co);
			if (lua_checkstack(thread, nres+1)) {
				int err;
				lua_pushboolean(thread, 1);  /* return 'true' to signal success */
				err = movevals(co, thread, nres);  /* move yielded values */
				if (err) {
					lua_pop(co, nres);  /* remove results anyway */
					lua_pop(thread, 1);  /* remove pushed 'true' that signals success */
					lua_pushboolean(thread, 0);
					lua_pushfstring(thread, "bad return value #%d (illegal type)", err);
				}
			} else {
				lua_pop(co, nres);  /* remove results anyway */
				lua_pushboolean(thread, 0);
				lua_pushliteral(thread, "too many results to resume");
			}
		} else {
			int err;
			lua_pushboolean(thread, 0);
			err = movevals(co, thread, 1);  /* move error message */
			if (err) {
				lua_pop(co, 1);  /* remove error anyway */
				lua_pushstring(thread, "bad error (illegal type)");
			}
		}
		lcu_setsyscorunning(sysco, 0);  /* frees 'co' if closed */
		lcuU_resumereqop(thread, loop, request);
	}
	else lcu_setsyscorunning(sysco, 0);  /* frees 'co' if closed */
}
static int k_setupwork (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_work_t *work = (uv_work_t *)request;
	lcu_SysCoro *sysco = lcu_checksysco(L, 1);
	lua_State *co = lcu_tosyscolua(sysco);
	int narg = lua_gettop(L)-1;
	int err;
	lcu_assert(request->data == L);
	if (lcu_issyscorunning(sysco)) {
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
	if (!lua_checkstack(co, narg)) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "too many arguments to resume");
		return 2;
	}
	err = movevals(L, co, narg);
	if (err) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "bad argument #%d (illegal type)", err);
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
	lcu_setsyscorunning(sysco, 1);
	return -1;  /* yield on success */
}
static int system_resume (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupwork, returnvalues);
}


LCUI_FUNC void lcuM_addcoroutf (lua_State *L) {
	static const luaL_Reg clsf[] = {
		{"__gc", coroutine_gc},
		{"close", coroutine_close},
		{"status", coroutine_status},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"coroutine", system_coroutine},
		{"resume", system_resume},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_SYSCOROCLS);
	lcuM_setfuncs(L, clsf, 0);
	lua_pop(L, 1);

	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
