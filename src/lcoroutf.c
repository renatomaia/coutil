#include "lmodaux.h"
#include "loperaux.h"
#include "lchaux.h"

#include <lmemlib.h>


#define CLASS_SYSCORO LCU_PREFIX"syscoro"

typedef struct lcu_SysCoro {
	int released;
	lua_State *thread;
	lua_State *coroutine;
} lcu_SysCoro;

#define tosysco(L) ((lcu_SysCoro *)luaL_checkudata(L,1,CLASS_SYSCORO))


static int doloaded (lua_State *L, lua_State *NL, int status) {
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		size_t len;
		const char *errmsg = lua_tolstring(NL, -1, &len);
		lua_pushnil(L);
		lua_pushlstring(L, errmsg, len);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	} else {
		lcu_SysCoro *sysco =
			(lcu_SysCoro *)lua_newuserdatauv(L, sizeof(lcu_SysCoro), 1);
		sysco->released = 0;
		sysco->thread = NULL;
		sysco->coroutine = NL;
		luaL_setmetatable(L, CLASS_SYSCORO);
		return 1;
	}
}

/* succ [, errmsg] = coroutine.load(chunk, chunkname, mode) */
static int coroutine_load (lua_State *L) {
	size_t l;
	const char *s = luamem_checkstring(L, 1, &l);
	const char *chunkname = luaL_optstring(L, 2, s);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	return doloaded(L, NL, status);
}

/* succ [, errmsg] = coroutine.loadfile(filepath, mode) */
static int coroutine_loadfile (lua_State *L) {
	const char *fpath = luaL_optstring(L, 1, NULL);
	const char *mode = luaL_optstring(L, 2, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return doloaded(L, NL, status);
}


static void freecoroutine (lcu_SysCoro *sysco) {
	lcu_assert(sysco->coroutine);
	lua_close(sysco->coroutine);
	sysco->coroutine = NULL;
}

static int closesysco (lua_State *L) {
	lcu_SysCoro *sysco = tosysco(L);
	if (sysco->released) return 0;
	if (sysco->thread == NULL) freecoroutine(sysco);
	sysco->released = 1;
	return 1;
}


/* getmetatable(coroutine).__{gc,close}(coroutine) */
static int coroutine_gc(lua_State *L) {
	closesysco(L);
	return 0;
}


/* succ = object:close() */
static int coroutine_close(lua_State *L) {
	int closed = closesysco(L);
	lua_pushboolean(L, closed);
	return 1;
}


/* status = coroutine:status() */
static int coroutine_status(lua_State *L) {
	lcu_SysCoro *sysco = tosysco(L);
	if (sysco->released) lua_pushliteral(L, "dead");
	else if (sysco->thread) lua_pushliteral(L, "running");
	else {
		lua_State *co = sysco->coroutine;
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
	lua_State *co = sysco->coroutine;
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
	lua_State *co = sysco->coroutine;
	lua_State *thread;
	request->data = sysco->thread;  /* restore 'lua_State' on conclusion */
	sysco->thread = NULL;
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
		if (sysco->released) freecoroutine(sysco);  /* must be freed while on the stack */
		lcuU_resumereqop(loop, request, nret);
	}
	else if (sysco->released) freecoroutine(sysco);
}
static int k_setupwork (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_work_t *work = (uv_work_t *)request;
	lcu_SysCoro *sysco = tosysco(L);
	lua_State *co = sysco->coroutine;
	int narg = lua_gettop(L)-1;
	int err;
	lcu_assert(request->data == L);
	if (sysco->thread) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "cannot resume running coroutine");
		return 2;
	}
	if (sysco->released || (lua_status(co) == LUA_OK && lua_gettop(co) == 0) ) {
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
	sysco->thread = L;
	return -1;  /* yield on success */
}
static int system_resume (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetreqopk(L, sched, k_setupwork, returnvalues, NULL);
}


LCUI_FUNC void lcuM_addcoroutf (lua_State *L) {
	static const luaL_Reg upvf[] = {
		{"resume", system_resume},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, upvf, LCU_MODUPVS);
}

LCUMOD_API int luaopen_coutil_coroutine (lua_State *L) {
	static const luaL_Reg meta[] = {
		{"__index", NULL},
		{"__gc", coroutine_gc},
		{"__close", coroutine_gc},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"load", coroutine_load},
		{"loadfile", coroutine_loadfile},
		{"close", coroutine_close},
		{"status", coroutine_status},
		{NULL, NULL}
	};
	lcuCS_tochannelmap(L);  /* map shall be GC after 'syscoro' on Lua close */
	luaL_newlib(L, modf);
	luaL_newmetatable(L, CLASS_SYSCORO);
	luaL_setfuncs(L, meta, 0);  /* add metamethods to metatable */
	lua_pushvalue(L, -2);  /* push library */
	lua_setfield(L, -2, "__index");  /* metatable.__index = library */
	lua_pop(L, 1);  /* pop metatable */

	return 1;
}
