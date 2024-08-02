#include "lmodaux.h"
#include "loperaux.h"
#include "lttyaux.h"
#include "lchaux.h"

#include <luamem.h>


typedef struct StateCoro {
	lua_CFunction results;
	lua_CFunction cancel;
	uv_work_t work;
	lua_State *L;
} StateCoro;

#define tostateco(L) ((StateCoro *)luaL_checkudata(L,1,LCU_STATECOROCLS))


static int doloaded (lua_State *L, lua_State *NL, int status) {
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		size_t len;
		const char *errmsg = lua_tolstring(NL, -1, &len);
		lua_pushboolean(L, 0);
		lua_pushlstring(L, errmsg, len);
		lua_close(lcuL_tomain(NL));
		return 2;  /* return nil plus error message */
	} else {
		StateCoro *stateco = lcuT_newudreq(L, StateCoro);
		stateco->L = NL;
		luaL_setmetatable(L, LCU_STATECOROCLS);
		return 1;
	}
}

/* succ [, errmsg] = coroutine.load(chunk, chunkname, mode) */
static int coroutine_load (lua_State *L) {
	size_t l;
	const char *s = luamem_checkarray(L, 1, &l);
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


static void freestate(StateCoro *stateco) {
	if (stateco->L) {
		lua_close(stateco->L);
		stateco->L = NULL;
	}
}

static int freepending(lua_State *L) {
	(void)L;
	return 1;  /* same as 'stateco->cancel == NULL' */
}


/* getmetatable(co).__{gc,close}(co) */
static int coroutine_gc(lua_State *L) {
	StateCoro *stateco = tostateco(L);
	if (stateco->work.type == UV_WORK) stateco->cancel = freepending;  /* lua_close */
	else freestate(stateco);
	return 0;
}


/* succ = coroutine.close(co) */
static int coroutine_close(lua_State *L) {
	StateCoro *stateco = tostateco(L);
	lua_State *co = stateco->L;
	int status;
	luaL_argcheck(L, stateco->work.type != UV_WORK, 1,
		"cannot close a running coroutine");
	lua_settop(L, 1);
	status = co ? lua_status(co) : LUA_OK;
	if (status == LUA_OK || status == LUA_YIELD) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
		lcuL_pushfrom(L, L, co, -1, "error");
	}
	coroutine_gc(L);
	return lua_gettop(L)-1;
}


static int suspended (lua_State *co) {
	switch (lua_status(co)) {
		case LUA_OK: return lua_gettop(co);
		case LUA_YIELD: return 1;
		default: return 0;
	}
}


/* status = coroutine.status(co) */
static int coroutine_status(lua_State *L) {
	StateCoro *stateco = tostateco(L);
	if (stateco->work.type == UV_WORK) lua_pushliteral(L, "running");
	else {
		lua_State *co = stateco->L;
		if (co && suspended(co)) lua_pushliteral(L, "suspended");
		else lua_pushliteral(L, "dead");
	}
	return 1;
}


/* succ [, errmsg] = system.resume(coroutine) */
static int returnvalues (lua_State *L) {
	return lua_gettop(L)-1;  /* return all except the coroutine (arg #1) */
}
static void uv_onworking(uv_work_t* request) {
	StateCoro *stateco = (StateCoro *)lcu_req2ud(request);
	lua_State *co = stateco->L;
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
	lua_State *L = (lua_State *)loop->data;
	uv_req_t *request = (uv_req_t *)work;
	StateCoro *stateco = (StateCoro *)lcu_req2ud(request);
	lua_State *co = stateco->L;
	lua_State *thread = lcuU_endudreq(loop, request);
	if (thread) {
		int nret;
		if (status == UV_ECANCELED) {
			lua_settop(co, lua_status(co) == LUA_OK);
			lua_pushboolean(thread, 0);
			lua_pushliteral(thread, "canceled");
			nret = 2;
		} else {
			int lstatus = (int)lua_tointeger(co, -1);
			nret = (int)lua_tointeger(co, -2);
			lua_pop(co, 2);  /* remove lstatus and nret values */
			if (lstatus == LUA_OK || lstatus == LUA_YIELD) {
				lua_pushboolean(thread, 1);  /* return 'true' to signal success */
				if (lcuL_movefrom(L, thread, co, nret, "return value") != LUA_OK) {
					lua_pop(co, nret);  /* remove results anyway */
					lua_pushboolean(thread, 0);
					lua_replace(thread, -3);  /* remove 'true' that signals success */
					nret = 2;
				}
			} else {
				lua_pushboolean(thread, 0);
				if (lcuL_pushfrom(L, thread, co, -1, "error") != LUA_OK) {
					lcuL_warnmsg(thread, "system.resume", lua_tostring(co, -1));
					lua_pop(co, 1);  /* remove error anyway */
				}
				nret = 2;
			}
		}
		lcuU_resumeudreq(loop, request, nret);
	} else {
		/* if not executed, remove arguments (LUA_OK when function is on stack) */
		lua_settop(co, status == UV_ECANCELED && lua_status(co) == LUA_OK);
	}
	if (stateco->cancel == freepending) {
		freestate(stateco);
		stateco->cancel = NULL;
	}
}
static int k_setupwork (lua_State *L,
                        uv_req_t *request,
                        uv_loop_t *loop,
                        lcu_Operation *op) {
	StateCoro *stateco = (StateCoro *)lua_touserdata(L, 1);
	lua_State *co = stateco->L;
	int narg = lua_gettop(L)-1;
	int err;
	lcu_assert(request == (uv_req_t *)&stateco->work);
	lcu_assert(op == NULL);
	if (lcuL_movefrom(NULL, co, L, narg, "argument") != LUA_OK) {
		lua_pushboolean(L, 0);
		if (lcuL_pushfrom(L, L, co, -1, "error") != LUA_OK)
			lcuL_warnmsg(L, "system.resume", lua_tostring(co, -1));
		lua_pop(co, 1);
		return 2;
	}
	err = uv_queue_work(loop, &stateco->work, uv_onworking, uv_onworked);
	if (err < 0) {
		lua_pop(co, narg);  /* restore coroutine stack */
		return lcuL_pusherrres(L, err);
	}
	return -1;  /* yield on success */
}
static int system_resume (lua_State *L) {
	StateCoro *stateco = tostateco(L);
	if (stateco->work.type == UV_WORK) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "cannot resume running coroutine");
		return 2;
	}
	if (stateco->L == NULL || !suspended(stateco->L)) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "cannot resume dead coroutine");
		return 2;
	}
	return lcuT_resetudreqk(L, lcu_getsched(L),
	                           (lcu_UdataRequest *)stateco,
	                           k_setupwork,
	                           returnvalues,
	                           NULL);
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
	(void)lcuTY_tostdiofd(L);  /* must be available to be copied to new threads */
	(void)lcuCS_tochannelmap(L);  /* map shall be GC after 'syscoro' on Lua close */
	luaL_newlib(L, modf);
	luaL_newmetatable(L, LCU_STATECOROCLS);
	luaL_setfuncs(L, meta, 0);  /* add metamethods to metatable */
	lua_pushvalue(L, -2);  /* push library */
	lua_setfield(L, -2, "__index");  /* metatable.__index = library */
	lua_pop(L, 1);  /* pop metatable */

	return 1;
}
