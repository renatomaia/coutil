#include "lthpool.h"
#include "lmodaux.h"
#include "lttyaux.h"
#include "lchaux.h"

#include <string.h>
#include <luamem.h>



#define TPOOLGCCLS	LCU_PREFIX"lcu_ThreadPool *"

static lcu_ThreadPool *tothreads (lua_State *L, int idx) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, idx, LCU_THREADSCLS);
	luaL_argcheck(L, *ref, idx, "closed threads");
	return *ref;
}

/* getmetatable(tpoolgc).__gc(pool) */
static int tpoolgc_gc (lua_State *L) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, 1, TPOOLGCCLS);
	lcuTP_closetpool(*ref);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, 1, LCU_THREADSCLS);
	luaL_argcheck(L, *ref, 1, "closed threads");
	if (*ref) {
		lua_pushlightuserdata(L, *ref);
		if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TUSERDATA) {
			lcu_assert(*ref == *((lcu_ThreadPool **)lua_touserdata(L, -1)));
			/* remove userdata from registry */
			lua_pushlightuserdata(L, *ref);
			lua_pushnil(L);
			lua_settable(L, LUA_REGISTRYINDEX);
			/* disable userdata GC metamethod */
			lua_pushnil(L);
			lua_setmetatable(L, -2);
			/* destroy thread pool on userdata */
			lcuTP_closetpool(*ref);
		}
		lua_pop(L, 1);
		*ref = NULL;
	}
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:resize(value) */
static int threads_resize (lua_State *L) {
	int err;
	lcu_ThreadPool *pool = tothreads(L, 1);
	int size = (int)luaL_checkinteger(L, 2);
	int create = lua_toboolean(L, 3);
	luaL_argcheck(L, size >= 0, 2, "size cannot be negative");
	err = lcuTP_resizetpool(pool, size, create);
	return lcuL_pushresults(L, 0, err);
}

/* succ [, errmsg] = threads:count(option) */
static int threads_count (lua_State *L) {
	lcu_ThreadCount count;
	lcu_ThreadPool *pool = tothreads(L, 1);
	const char *opt = luaL_checkstring(L, 2);
	size_t len = strlen(opt);
	luaL_checkstack(L, len, "too many values to return");
	lcuTP_counttpool(pool, &count, len < 6 ? opt : "earpsn");
	lua_settop(L, 2);
	for (; *opt; opt++) switch (*opt) {
		case 'e': lua_pushinteger(L, count.expected); break;
		case 'a': lua_pushinteger(L, count.actual); break;
		case 'r': lua_pushinteger(L, count.running); break;
		case 'p': lua_pushinteger(L, count.pending); break;
		case 's': lua_pushinteger(L, count.suspended); break;
		case 'n': lua_pushinteger(L, count.numoftasks); break;
		default: return luaL_error(L, "bad option (got '%c')", (int)*opt);
	}
	return lua_gettop(L)-2;
}

static int returntoperrmsg (lua_State *L, lua_State *NL) {
	lua_pushboolean(L, 0);
	if (lcuL_pushfrom(NULL, L, NL, -1, "error") != LUA_OK)
		lcuL_warnmsg(L, "threads.dostring", lua_tostring(NL, -1));
	lua_close(lcuL_tomain(NL));
	return 2;  /* return false plus error message */
}

static int dochunk (lua_State *L,
                    lcu_ThreadPool *pool,
                    lua_State *NL,
                    int status,
                    int narg) {
	int top;
	if (status != LUA_OK) return returntoperrmsg(L, NL);
	top = lua_gettop(L);
	status = lcuL_movefrom(NULL, NL, L, top > narg ? top-narg : 0, "argument");
	if (status != LUA_OK) return returntoperrmsg(L, NL);
	status = lcuTP_addtpooltask(pool, NL);
	if (status) {
		lua_close(lcuL_tomain(NL));
		return lcuL_pusherrres(L, status);
	}
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:dostring(chunk [, chunkname [, mode, ...]]) */
static int threads_dostring (lua_State *L) {
	lcu_ThreadPool *pool = tothreads(L, 1);
	size_t l;
	const char *s = luamem_checkarray(L, 2, &l);
	const char *chunkname = luaL_optstring(L, 3, s);
	const char *mode = luaL_optstring(L, 4, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	return dochunk(L, pool, NL, status, 4);
}

/* succ [, errmsg] = threads:dofile([path [, mode, ...]]) */
static int threads_dofile (lua_State *L) {
	lcu_ThreadPool *pool = tothreads(L, 1);
	const char *fpath = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return dochunk(L, pool, NL, status, 3);
}

/* threads [, errmsg] = system.threads([size]) */
static int threads_create (lua_State *L) {
	lcu_ThreadPool *pool;
	if (lua_gettop(L) > 0) {
		int err, size = (int)luaL_checkinteger(L, 1);
		lcu_ThreadPool **ref =
			(lcu_ThreadPool **)lua_newuserdatauv(L, sizeof(lcu_ThreadPool *), 0);
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		err = lcuTP_createtpool(ref, allocf, allocud);
		if (err) return lcuL_pusherrres(L, err);
		pool = *ref;
		if (size > 0) {
			err = lcuTP_resizetpool(pool, size, 1);
			if (err) {
				lcuTP_closetpool(pool);
				return lcuL_pusherrres(L, err);
			}
		}
		luaL_setmetatable(L, TPOOLGCCLS);
		lua_pushlightuserdata(L, pool);
		lua_insert(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	} else {
		int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
		if (type == LUA_TNIL) return 1;
		pool = *((lcu_ThreadPool **)lua_touserdata(L, -1));
		lua_pop(L, 1);
	}
	{
		lcu_ThreadPool **ref =
			(lcu_ThreadPool **)lua_newuserdatauv(L, sizeof(lcu_ThreadPool *), 0);
		*ref = pool;
		luaL_setmetatable(L, LCU_THREADSCLS);
	}
	return 1;
}


LCUMOD_API int luaopen_coutil_threads (lua_State *L) {
	static const luaL_Reg poolrefmt[] = {
		{"__gc", tpoolgc_gc},
		{NULL, NULL}
	};
	static const luaL_Reg threadsmt[] = {
		{"__index", NULL},
		{"__close", threads_close},
		{NULL, NULL}
	};
	static const luaL_Reg modulef[] = {
		{"create", threads_create},
		{"close", threads_close},
		{"resize", threads_resize},
		{"count", threads_count},
		{"dostring", threads_dostring},
		{"dofile", threads_dofile},
		{NULL, NULL}
	};
	(void)lcuTY_tostdiofd(L);  /* must be available to be copied to new threads */
	(void)lcuCS_tochannelmap(L);  /* map shall be GC after 'threads' on Lua close */
	luaL_newlib(L, modulef);
	luaL_newmetatable(L, TPOOLGCCLS)  /* metatable for tpool sentinel */;
	luaL_setfuncs(L, poolrefmt, 0);  /* add metamethods to metatable */
	lua_pop(L, 1);  /* pop metatable */
	luaL_newmetatable(L, LCU_THREADSCLS);  /* metatable for thread pools */
	luaL_setfuncs(L, threadsmt, 0);  /* add metamethods to metatable */
	lua_pushvalue(L, -2);  /* push library */
	lua_setfield(L, -2, "__index");  /* metatable.__index = library */
	lua_pop(L, 1);  /* pop metatable */
	return 1;
}
