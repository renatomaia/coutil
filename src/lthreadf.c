#include "lmodaux.h"

#include <lmemlib.h>


/*** lsyslib.h ****************************************************************/

#define LCU_THREADPOOLREGKEY LCU_PREFIX"lcu_ThreadPool *parentpool"
#define LCU_THREADPOOLCLS LCU_PREFIX"lcu_ThreadPool"
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

LCULIB_API int lcu_initthreads (lcu_ThreadPool *pool);

LCULIB_API void lcu_terminatethreads (lcu_ThreadPool *pool);

LCULIB_API int lcu_resizethreads (lcu_ThreadPool *pool, int size, int create);

LCULIB_API int lcu_addthreads (lcu_ThreadPool *pool, lua_State *L);

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool, lcu_ThreadCount what);



#define lcu_isthreads(L,i)  (lcu_tothreads(L, i) != NULL)

LCULIB_API void lcu_pushthreads (lua_State *L, lcu_ThreadPool *pool);

LCULIB_API lcu_ThreadPool *lcu_checkthreads (lua_State *L, int idx);

LCULIB_API lcu_ThreadPool *lcu_tothreads (lua_State *L, int idx);

/*** lsyslib.c ****************************************************************/

#define LCU_NEXTSTATEREGKEY LCU_PREFIX"lua_State *nexttask"

typedef struct StateQ {
	lua_State *head;
	lua_State *tail;
} StateQ;

static void initstateq (StateQ *q) {
	q->head = NULL;
	q->tail = NULL;
}

static void enqueuestateq (StateQ *q, lua_State *L) {
	if (q->head) {
		lcu_assert(lua_checkstack(L, 1));
		lua_pushlightuserdata(q->tail, L);
		lua_setfield(q->tail, LUA_REGISTRYINDEX, LCU_NEXTSTATEREGKEY);
	} else {
		q->head = L;
	}
	q->tail = L;
}

static lua_State *dequeuestateq (StateQ *q) {
	lua_State *L = q->head;
	if (L) {
		lcu_assert(lua_checkstack(L, 1));
		if (L == q->tail) {
			q->head = NULL;
			q->tail = NULL;
		} else {
			lua_getfield(L, LUA_REGISTRYINDEX, LCU_NEXTSTATEREGKEY);
			q->head = (lua_State *)lua_touserdata(L, -1);
			lua_pop(L, 1);
		}
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_NEXTSTATEREGKEY);
	}
	return L;
}


//typedef struct ThreadSet {
//	union {
//		int count;
//		uv_thread_t tid;
//	} value;
//	ThreadSet *prev;
//	ThreadSet *next;
//} ThreadSet;
//
//static void initthreadset (ThreadSet *set) {
//	set->value.count = 0;
//	set->prev = set;
//	set->next = set;
//}
//
//static void addthreadset (ThreadSet *set, ThreadSet *entry) {
//	ThreadSet *first = set->next;
//	lcu_assert(entry->prev == entry);
//	lcu_assert(entry->next == entry);
//	entry->next = first;
//	set->next = entry;
//	first->prev = entry;
//	entry->prev = set;
//}
//
//static void removethreadset (ThreadSet *entry) {
//	ThreadSet *prev = entry->prev;
//	ThreadSet *next = entry->next;
//	prev->next = next;
//	next->prev = prev;
//	entry->prev = entry;
//	entry->next = entry;
//}


struct lcu_ThreadPool {
	uv_mutex_t mutex;
	uv_cond_t onwork;
	uv_cond_t onterm;
	int detached;
	int size;
	int threads;
	int idle;
	int running;
	int pending;
	StateQ queue;
};

LCULIB_API int lcu_initthreads (lcu_ThreadPool *pool) {
	int err = uv_mutex_init(&pool->mutex);
	if (err) goto mutex_err;
	err = uv_cond_init(&pool->onwork);
	if (err) goto onworkcond_err;
	err = uv_cond_init(&pool->onterm);
	if (err) goto ontermcond_err;

	pool->detached = 0;
	pool->size = 0;
	pool->threads = 0;
	pool->idle = 0;
	pool->running = 0;
	pool->pending = 0;
	initstateq(&pool->queue);
	return 0;

	ontermcond_err:
	uv_cond_destroy(&pool->onwork);
	onworkcond_err:
	uv_mutex_destroy(&pool->mutex);
	mutex_err:
	return err;
}

static int hasextraidle_mx (lcu_ThreadPool *pool) {
	return pool->threads > pool->size && pool->idle > 0;
}

static void checkhalted_mx (lcu_ThreadPool *pool) {
	if (pool->running == 0 && pool->pending == 0 && pool->size > 0) {
		pool->size = 0;
		uv_cond_signal(&pool->onwork);
	}
}

static void runthread (void *arg) {
	lcu_ThreadPool *pool = (lcu_ThreadPool *)arg;
	lua_State *L = NULL;

	uv_mutex_lock(&pool->mutex);
	while (1) {
		int status;
		while (1) {
			if (pool->threads > pool->size) {
				goto thread_end;
			} else if (pool->pending) {
				pool->pending--;
				L = dequeuestateq(&pool->queue);
				break;
			} else if (pool->detached && pool->running == 0) {  /* if halted? */
				pool->size = 0;
			} else {
				pool->idle++;
				uv_cond_wait(&pool->onwork, &pool->mutex);
				pool->idle--;
			}
		}
		pool->running++;
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
			enqueuestateq(&pool->queue, L);
			L = NULL;
		}
	}
	thread_end:
	pool->threads--;
	if (hasextraidle_mx(pool)) {
		uv_cond_signal(&pool->onwork);
	} else if (pool->detached) {
		if (pool->threads == 0) uv_cond_signal(&pool->onterm);
		else checkhalted_mx(pool);
	}
	uv_mutex_unlock(&pool->mutex);
}

LCULIB_API void lcu_terminatethreads (lcu_ThreadPool *pool) {
	int pending;
	StateQ queue;

	uv_mutex_lock(&pool->mutex);
	pool->detached = 1;
	if (pool->threads > 0) {
		checkhalted_mx(pool);
		uv_cond_wait(&pool->onterm, &pool->mutex);
	}
	pending = pool->pending;
	queue = pool->queue;
	/* DEBUG: initstateq(&pool->queue); */
	/* DEBUG: pool->pending = 0; */
	uv_mutex_unlock(&pool->mutex);

	if (pending > 0) {
		lua_State *L;
		while ((L = dequeuestateq(&queue))) lua_close(L);
	}
	uv_cond_destroy(&pool->onterm);
	uv_cond_destroy(&pool->onwork);
	uv_mutex_destroy(&pool->mutex);
}

LCULIB_API int lcu_resizethreads (lcu_ThreadPool *pool, int size, int create) {
	int newthreads, err = 0;

	uv_mutex_lock(&pool->mutex);
	newthreads = size-pool->size;
	pool->size = size;
	if (newthreads > 0) {
		if (!create && newthreads > pool->pending) newthreads = pool->pending;
		while (newthreads--) {
			uv_thread_t tid;
			err = uv_thread_create(&tid, runthread, pool);
			if (err) break;
			pool->threads++;
		}
	} else if (hasextraidle_mx(pool)) {
		uv_cond_signal(&pool->onwork);
	}
	uv_mutex_unlock(&pool->mutex);

	return err;
}

LCULIB_API int lcu_addthreads (lcu_ThreadPool *pool, lua_State *L) {
	int err = 0;
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	lua_pushlightuserdata(L, pool);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);

	uv_mutex_lock(&pool->mutex);
	if (pool->idle > 0) {
		uv_cond_signal(&pool->onwork);
	} else if (pool->threads < pool->size) {
		uv_thread_t tid;
		err = uv_thread_create(&tid, runthread, pool);
		if (err) goto newthread_err;
		pool->threads++;
	}
	enqueuestateq(&pool->queue, L);
	pool->pending++;

	newthread_err:
	uv_mutex_unlock(&pool->mutex);

	return err;
}

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool, lcu_ThreadCount what) {
	int count = 0;

	uv_mutex_lock(&pool->mutex);
	switch (what) {
		case LCU_THREADSSIZE: count = pool->size; break;
		case LCU_THREADSCOUNT: count = pool->threads; break;
		case LCU_THREADSRUNNING: count = pool->running; break;
		case LCU_THREADSPENDING: count = pool->pending; break;
		case LCU_THREADSSUSPENDED: count = 0; break;
		case LCU_THREADSTASKS: count = pool->running+pool->pending; break;
	}
	uv_mutex_unlock(&pool->mutex);

	return count;
}



LCULIB_API void lcu_pushthreads (lua_State *L, lcu_ThreadPool *pool) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)lua_newuserdata(L, sizeof(lcu_ThreadPool*));
	*ref = pool;
	luaL_setmetatable(L, LCU_THREADSCLS);
}

LCULIB_API lcu_ThreadPool *lcu_checkthreads (lua_State *L, int idx) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, idx, LCU_THREADSCLS);
	luaL_argcheck(L, *ref, idx, "closed threads");
	return *ref;
}

LCULIB_API lcu_ThreadPool *lcu_tothreads (lua_State *L, int idx) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_testudata(L, idx, LCU_THREADSCLS);
	return ref ? *ref : NULL;
}

/******************************************************************************/

/* getmetatable(threads).__gc(threads) */
static int threadpool_gc (lua_State *L) {
	lcu_ThreadPool *pool = (lcu_ThreadPool *)luaL_checkudata(L, 1, LCU_THREADPOOLCLS);
	lcu_terminatethreads(pool);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, 1, LCU_THREADSCLS);
	lua_pushboolean(L, *ref != NULL);
	if (*ref) {
		lua_pushlightuserdata(L, *ref);
		if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TUSERDATA) {
			lcu_assert(*ref == lua_touserdata(L, -1));
			lcu_terminatethreads(*ref);
			lua_pushnil(L);
			lua_setmetatable(L, -2);
			lua_pushlightuserdata(L, *ref);
			lua_pushnil(L);
			lua_settable(L, LUA_REGISTRYINDEX);
		}
		lua_pop(L, 1);
		*ref = NULL;
	}
	return 1;
}

/* succ [, errmsg] = threads:resize(value) */
static int threads_resize (lua_State *L) {
	int err;
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	int size = (int)luaL_checkinteger(L, 2);
	int create = lua_toboolean(L, 3);
	luaL_argcheck(L, size >= 0, 2, "size cannot be negative");
	err = lcu_resizethreads(pool, size, create);
	if (err) return lcuL_pusherrres(L, err);
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:count([option]) */
static int threads_count (lua_State *L) {
	static const lcu_ThreadCount options[] = { LCU_THREADSSIZE,
	                                           LCU_THREADSCOUNT,
	                                           LCU_THREADSRUNNING,
	                                           LCU_THREADSPENDING,
	                                           LCU_THREADSSUSPENDED,
	                                           LCU_THREADSTASKS };
	static const char *const optnames[] = { "size",
	                                        "threads",
	                                        "running",
	                                        "pending",
	                                        "suspended",
	                                        "tasks",
	                                        NULL };
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	int option = luaL_checkoption(L, 2, "tasks", optnames);
	int value = lcu_countthreads(pool, options[option]);
	lua_pushinteger(L, value);
	return 1;
}

static int dochunk (lua_State *L, lcu_ThreadPool *pool, lua_State *NL, int status, int narg) {
	int err;
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
	err = lcu_addthreads(pool, NL);
	if (err) {
		lua_close(NL);
		return lcuL_pusherrres(L, err);
	}
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

/* succ [, errmsg] = threads:dofile([path [, mode, ...]]) */
static int threads_dofile (lua_State *L) {
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	const char *fpath = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return dochunk(L, pool, NL, status, lua_gettop(L)-3);
}

/* threads [, errmsg] = system.threads([size]) */
static int system_threads (lua_State *L) {
	lcu_ThreadPool *pool;
	if (lua_gettop(L) > 0) {
		int err, size = (int)luaL_checkinteger(L, 1);
		pool = (lcu_ThreadPool *)lua_newuserdata(L, sizeof(lcu_ThreadPool));
		err = lcu_initthreads(pool);
		if (err) return lcuL_pusherrres(L, err);
		if (size > 0) {
			err = lcu_resizethreads(pool, size, 1);
			if (err) {
				lcu_terminatethreads(pool);
				return lcuL_pusherrres(L, err);
			}
		}
		luaL_setmetatable(L, LCU_THREADPOOLCLS);
		lua_pushlightuserdata(L, pool);
		lua_insert(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
		lcu_pushthreads(L, pool);
	} else {
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
		if (lua_isnil(L, -1)) return 1;
		pool = (lcu_ThreadPool *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		lcu_pushthreads(L, pool);
	}
	return 1;
}

LCUI_FUNC void lcuM_addthreadc (lua_State *L) {
	static const luaL_Reg poolclsf[] = {
		{"__gc", threadpool_gc},
		{NULL, NULL}
	};
	static const luaL_Reg refclsf[] = {
		{"close", threads_close},
		{"resize", threads_resize},
		{"count", threads_count},
		{"dostring", threads_dostring},
		{"dofile", threads_dofile},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_THREADPOOLCLS);
	lcuM_setfuncs(L, poolclsf, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, LCU_THREADSCLS);
	lcuM_setfuncs(L, refclsf, 0);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addthreadf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"threads", system_threads},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
}
