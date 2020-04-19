#include <lmemlib.h>

#include "lmodaux.h"


/*** lsyslib.h ****************************************************************/

#define LCU_THREADPOOLREGKEY LCU_PREFIX"lcu_ThreadPool"
#define LCU_THREADSCLS LCU_PREFIX"threads"

typedef enum lcu_ThreadCount {
	LCU_THREADSSIZE,
	LCU_THREADSCOUNT,
	LCU_THREADSRUNNING,
	LCU_THREADSPENDING,
	LCU_THREADSSUSPENDED,
	LCU_THREADSTASKS
} lcu_ThreadCount;

typedef struct lcu_ThreadPool lcu_ThreadPool;

LCULIB_API int lcu_newthreads (lcu_ThreadPool **pool,
                               lua_Alloc allocf,
                               void *allocud);

LCULIB_API void lcu_detachthreads (lcu_ThreadPool *pool);

LCULIB_API int lcu_resizethreads (lcu_ThreadPool *pool, int newsize);

LCULIB_API void lcu_addthreads (lcu_ThreadPool *pool, lua_State *L);

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool, lcu_ThreadCount what);



#define lcu_isthreads(L,i)  (lcu_tothreads(L, i) != NULL)

LCULIB_API void lcu_pushthreads (lcu_ThreadPool *pool, int shalldetach);

LCULIB_API lcu_ThreadPool *lcu_checkthreads (lua_State *L, int idx);

LCULIB_API lcu_ThreadPool *lcu_tothreads (lua_State *L, int idx);

LCULIB_API int lcu_closethreads (lua_State *L, int idx);

/*** lsyslib.c ****************************************************************/

typedef struct StateQ {
	lua_State *head;
	lua_State *tail;
} StateQ;

static void initqueue (StateQ *q) {
	q->head = NULL;
	q->tail = NULL;
}

static void enqueuestate (StateQ *q, lua_State *L) {
	if (q->head) {
		lcu_assert(lua_checkstack(L, 1));
		lua_pushlightuserdata(q->tail, L);
		lua_setfield(q->tail, LUA_REGISTRYINDEX, LCU_REGFIELDNEXT);
	} else {
		q->head = L;
	}
	q->tail = L;
}

static lua_State *dequeuestate (StateQ *q) {
	lua_State *L = q->head;
	if (L) {
		lcu_assert(lua_checkstack(L, 1));
		if (L == q->tail) {
			q->head = NULL;
			q->tail = NULL;
		} else {
			lua_getfield(L, LUA_REGISTRYINDEX, LCU_REGFIELDNEXT);
			q->head = (lua_State *)lua_touserdata(L, -1);
			lua_pop(L, 1);
		}
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_REGFIELDNEXT);
	}
	return L;
}


struct lcu_ThreadPool {
	int detached;
	lua_Alloc allocf;
	void *allocud;

	uv_mutex_t mutex;
	uv_cond_t idle;
	int size;
	int threads;
	int running;
	int pending;
	StateQ queue;
};

LCULIB_API int lcu_newthreads (lcu_ThreadPool **pool,
                               lua_Alloc allocf,
                               void *allocud) {
	int err;
	*pool = (lcu_ThreadPool *)allocf(allocud, NULL, 0, sizeof(lcu_ThreadPool));
	if (*pool == NULL) return UV_ENOMEM;
	err = uv_mutex_init(&(*pool)->mutex);
	if (err) goto mutex_err;
	err = uv_cond_init(&(*pool)->idle);
	if (err) goto cond_err;

	(*pool)->allocf = allocf;
	(*pool)->allocud = allocud;
	(*pool)->detached = 0;
	(*pool)->size = 0;
	(*pool)->threads = 0;
	(*pool)->running = 0;
	(*pool)->pending = 0;
	initqueue(&(*pool)->queue);
	return 0;

	cond_err:
	uv_mutex_destroy(&(*pool)->mutex);
	mutex_err:
	allocf(allocud, *pool, sizeof(lcu_ThreadPool), 0);
	return err;
}

static void destroypool (lcu_ThreadPool *pool) {
	uv_cond_destroy(&(*pool)->idle);
	uv_mutex_destroy(&(*pool)->mutex);
	pool->allocf(pool->allocud, pool, sizeof(lcu_ThreadPool), 0);
}

static int shalldestroy (lcu_ThreadPool *pool) {
	if (pool->detached && !pool->running && (!pool->pending || !pool->size)) {
		lua_State *L;
		while (L = dequeuestate(&pool->queue)) lua_close(L);
		pool->pending = 0;
		pool->size = 0;
		if (pool->threads) uv_cond_signal(&pool->idle);
		else return 1;
	}
	return 0;
}

static void runthread (void *arg) {
	lcu_ThreadPool *pool = (lcu_ThreadPool *)arg;
	lua_State *L = NULL;
	int destroy = 0;
	uv_mutex_lock(&pool->mutex);
	while (1) {
		int status;
		do {
			if (pool->pending) {
				pool->pending--;
				pool->running++;
				L = dequeuestate(&pool->queue);
			} else if (pool->threads > pool->size) {
				pool->threads--;
				goto termination;
			} else {
				uv_cond_wait(&pool->idle, &pool->mutex);
			}
		} while (!L)
		uv_mutex_unlock(&pool->mutex);

		status = lua_resume(L, NULL, lua_status(L) == LUA_OK ? lua_gettop(L)-1 : 0);
		if (status == LUA_YIELD) {
			lua_settop(L, 0);  /* discard returned values */
		} else {
			lua_close(L);
			L = NULL;
		}

		uv_mutex_lock(&pool->mutex);
		pool->running--;
		if (L) {
			pool->pending++;
			enqueuestate(&pool->queue, L);
			L = NULL;
		}
	}
	termination:
	if (pool->threads > pool->size) uv_cond_signal(&pool->idle);
	else destroy = shalldestroy(pool);
	uv_mutex_unlock(&pool->mutex);
	if (destroy) destroypool(pool);
}

LCULIB_API void lcu_detachthreads (lcu_ThreadPool *pool) {
	int dodestroy = 0;
	uv_mutex_lock(&pool->mutex);
	pool->detached = 1;
	dodestroy = shalldestroy(pool);
	uv_mutex_unlock(&pool->mutex);
	if (dodestroy) destroypool(pool);
}

LCULIB_API int lcu_resizethreads (lcu_ThreadPool *pool, int newsize) {
	uv_thread_t tid;
	int extra, err = 0;
	uv_mutex_lock(&pool->mutex);
	extra = newsize-pool->size;
	pool->size = newsize;
	if (extra > 0) {
		if (extra > pool->pending) extra = pool->pending;
		while (extra--) {
			err = uv_thread_create(&tid, runthread, pool);
			if (err) break;
			pool->threads++;
		}
	} else if (pool->threads > pool->size && pool->threads > pool->running) {
		uv_cond_signal(&pool->idle);
	}
	uv_mutex_unlock(&pool->mutex);
	return err;
}

LCULIB_API void lcu_addthreads (lcu_ThreadPool *pool, lua_State *L) {
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	lua_pushlightuserdata(L, pool);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
	uv_mutex_lock(&pool->mutex);
	pool->pending++;
	enqueuestate(&pool->queue, L);
	if (pool->threads == pool->running && pool->threads < pool->size) {
		uv_thread_t tid;
		int err = uv_thread_create(&tid, runthread, pool);
		if (!err) pool->threads++;
	}
	else uv_cond_signal(&pool->idle);
	uv_mutex_unlock(&pool->mutex);
}

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool, lcu_ThreadCount what) {
	int count = 0;
	uv_mutex_lock(&pool->mutex);
	switch (what) {
		case LCU_THREADSSIZE: count = pool->size;
		case LCU_THREADSCOUNT: count = pool->threads;
		case LCU_THREADSRUNNING: count = pool->running;
		case LCU_THREADSPENDING: count = pool->pending;
		case LCU_THREADSSUSPENDED: count = 0;
		case LCU_THREADSTASKS: count = pool->running+pool->pending;
	}
	uv_mutex_unlock(&pool->mutex);
	return count;
}



#define FLAG_CLOSED 0x01
#define FLAG_DEATCHED 0x02

typedef struct LThreads {
	int flags;
	lcu_ThreadPool *pool;
} LThreads;

LCULIB_API void lcu_pushthreads (lcu_ThreadPool *pool, int shalldetach) {
	LThreads *lpool = (LThreads *)lua_newuserdata(L, sizeof(LThreads));
	lpool->flags = shalldetach ? 0 : FLAG_DEATCHED;
	lpool->pool = pool;
	luaL_setmetatable(L, -1, LCU_THREADSCLS);
}

LCULIB_API lcu_ThreadPool *lcu_checkthreads (lua_State *L, int idx) {
	LThreads *lpool = (LThreads *)luaL_checkudata(L, idx, LCU_THREADSCLS);
	return lpool->pool;
}

LCULIB_API lcu_ThreadPool *lcu_tothreads (lua_State *L, int idx) {
	LThreads *lpool = (LThreads *)luaL_testudata(L, idx, LCU_THREADSCLS);
	return lpool ? lpool->pool : NULL;
}

LCULIB_API int lcu_closethreads (lua_State *L, int idx) {
	LThreads *lpool = (LThreads *)luaL_checkudata(L, idx, LCU_THREADSCLS);
	if (!lcuL_maskflag(lpool, FLAG_CLOSED)) {
		lcuL_setflag(lpool, FLAG_CLOSED);
		if (!lcuL_maskflag(lpool, FLAG_DEATCHED)) {
			lcuL_setflag(lpool, FLAG_DEATCHED);
			lcu_detachthreads(lpool->pool);
		}
		return 1;
	}
	return 0;
}

/******************************************************************************/

/* getmetatable(threads).__gc(threads) */
static int threads_gc (lua_State *L) {
	lcu_closethreads(L, 1);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	int closed = lcu_closethreads(L, 1);
	lua_pushboolean(L, closed);
	return 1;
}

/* succ [, errmsg] = threads:resize(value) */
static int threads_resize (lua_State *L) {
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	int size = (int)luaL_checkinteger(L, 2);
	int err = lcu_resizethreads(pool, size);
	if (err) return lcuL_pusherrres(L, err);
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:getcount([option]) */
static int threads_getcount (lua_State *L) {
	static const lcu_ThreadCount options[] = { LCU_THREADSSIZE,
	                                           LCU_THREADSCOUNT,
	                                           LCU_THREADSRUNNING,
	                                           LCU_THREADSPENDING,
	                                           LCU_THREADSSUSPENDED,
	                                           LCU_THREADSTASKS };
	static const char *const optnames[] = { "size",
	                                        "thread",
	                                        "running",
	                                        "pending",
	                                        "suspended",
	                                        "tasks",
	                                        NULL };
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	int option = luaL_checkoption(L, 2, "tasks", optnames);
	lua_pushnumber(L, lcu_countthreads(pool, options[option]));
	return 1;
}

static int dochunk (lua_State *L, lcu_ThreadPool *pool, lua_State *NL, int status, int narg) {
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		const char *errmsg = lua_tostring(NL, -1);
		lua_pushnil(L);
		lua_pushstring(L, errmsg);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	err = lcuL_movevals(L, NL, narg);
	if (err) {
		lua_close(NL);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "bad argument #%d (illegal type)", err);
		return 2;
	}
	lcu_addthreads(pool, NL);
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:dostring(chunk [, chunkname [, mode, ...]]) */
static int threads_dostring (lua_State *L) {
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	size_t l;
	const char *s = luamem_checkstring(L, 2, &l);
	const char *chunkname = luaL_optstring(L, 3, s);
	const char *mode = luaL_optstring(L, 4, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadbufferx(NL, s, l, chunkname, mode);
	return dochunk(L, pool, NL, status, lua_gettop(L)-4);
}

/* succ [, errmsg] = threads:dofile(filepath [, mode, ...]) */
static int threads_dofile (lua_State *L) {
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	const char *fpath = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return dochunk(L, pool, NL, status, lua_gettop(L)-3);
}

/* threads [, errmsg] = system.threads([size]) */
static int system_threads (lua_State *L) {
	int shalldetach;
	lcu_ThreadPool *pool;
	if (lua_gettop(L) == 0) {
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
		if (lua_isnil(L, -1)) return 1;
		pool = (lcu_ThreadPool *)lua_touserdata(L, -1);
		shalldetach = 0;
	} else {
		int size = (int)luaL_checkinteger(L, 1);
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		int err = lcu_newthreads(&pool, allocf, allocud);
		if (err) return lcuL_pusherrres(L, err);
		if (size > 0) {
			err = lcu_resizethreads(pool, size);
			if (err) {
				lcu_detachthreads(pool);
				return lcuL_pusherrres(L, err);
			}
		}
		shalldetach = 1;
	}
	lcu_pushthreads(L, pool, shalldetach);
	return 1;
}

LCUI_FUNC void lcuM_addcthreadc (lua_State *L) {
	static const luaL_Reg clsf[] = {
		{"__gc", threads_gc},
		{"close", threads_close},
		{"resize", threads_resize},
		{"getcount", threads_getcount},
		{"dostring", threads_dostring},
		{"dofile", threads_dofile},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_THREADSCLS);
	lcuM_setfuncs(L, clsf, 0);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addthreadf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"threads", system_threads},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
}
