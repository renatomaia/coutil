#include "lmodaux.h"

#include <lmemlib.h>


/******************************************************************************/

#define LCU_THREADPOOLREGKEY LCU_PREFIX"lcu_ThreadPool *parentpool"
#define LCU_THREADPOOLCLS LCU_PREFIX"lcu_ThreadPool"
#define LCU_THREADSCLS LCU_PREFIX"threads"

typedef struct lcu_ThreadCount {
	int expected;
	int actual;
	int running;
	int pending;
	int suspended;
	int numoftasks;
} lcu_ThreadCount;

typedef struct lcu_ThreadPool lcu_ThreadPool;

LCULIB_API int lcu_initthreads (lcu_ThreadPool *pool);

LCULIB_API void lcu_terminatethreads (lcu_ThreadPool *pool);

LCULIB_API int lcu_resizethreads (lcu_ThreadPool *pool, int size, int create);

LCULIB_API int lcu_addthreads (lcu_ThreadPool *pool, lua_State *L);

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool,
                                 lcu_ThreadCount *count,
                                 const char *what);

#define lcu_isthreads(L,i)  (lcu_tothreads(L, i) != NULL)

LCULIB_API void lcu_pushthreads (lua_State *L, lcu_ThreadPool *pool);

LCULIB_API lcu_ThreadPool *lcu_checkthreads (lua_State *L, int idx);

LCULIB_API lcu_ThreadPool *lcu_tothreads (lua_State *L, int idx);



#define LCU_SYNCPORTREGISTRY LCU_PREFIX"SyncPortRegistry"
#define LCU_SYNCPORTCLS LCU_PREFIX"syncport"

#define LCU_SYNCPORTIN  0x01
#define LCU_SYNCPORTOUT 0x02

typedef struct lcu_SyncPort lcu_SyncPort;

LCULIB_API lcu_SyncPort *lcu_newsyncport (lua_State *L);

LCULIB_API void lcu_refsyncport (lcu_SyncPort *port);

LCULIB_API void lcu_unrefsyncport (lcu_SyncPort *port);

#define lcu_tosyncportref(L,I) ((lcu_SyncPort **)luaL_testudata(L,I,LCU_SYNCPORTCLS))

LCULIB_API lcu_SyncPort *lcu_checksyncport (lua_State *L, int idx);

LCULIB_API void lcu_pushsyncport (lua_State *L, lcu_SyncPort *port);

LCULIB_API int lcu_closesyncport (lua_State *L, int idx);

LCULIB_API int lcu_pushsyncportfrom (lua_State *to, lua_State *from, int arg);

LCULIB_API int lcu_matchsyncport (lcu_SyncPort **portref,
                                  const char *endname,
                                  lua_State *L);


#define LCU_CHANNELCLS LCU_PREFIX"channel"

/******************************************************************************/

/*
 * Thread Pools
 */

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
	int tasks;
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
	pool->tasks = 0;
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
	if (pool->tasks == 0 && pool->size > 0) {
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
			} else if (pool->detached && pool->tasks == 0) {  /* if halted? */
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
			lcu_SyncPort **portref = lcu_tosyncportref(L, 1);
			if (portref && *portref) {
				const char *endname = lua_tostring(L, 2);
				if (endname == NULL) endname = "";
				if (!lcu_matchsyncport(portref, endname, L)) L = NULL;
			} else {
				lua_settop(L, 0);  /* discard returned values */
			}
		} else {
			/* avoid 'pool->tasks--' */
			lua_settop(L, 0);
			lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
			lua_pushnil(L);
			lua_setmetatable(L, -2);
			lua_close(L);
			L = NULL;
		}

		uv_mutex_lock(&pool->mutex);
		pool->running--;
		if (L) {
			pool->pending++;
			enqueuestateq(&pool->queue, L);
			L = NULL;
		} else if (status != LUA_YIELD) {
			pool->tasks--;
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

static int collectthreadpool (lua_State *L) {
	lcu_ThreadPool *pool = *((lcu_ThreadPool **)lua_touserdata(L, 1));
	uv_mutex_lock(&pool->mutex);
	pool->tasks--;
	uv_mutex_unlock(&pool->mutex);
	return 0;
}

LCULIB_API int lcu_addthreads (lcu_ThreadPool *pool, lua_State *L) {
	lcu_ThreadPool **poolref;
	int err = 0;
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	poolref = (lcu_ThreadPool **)lua_newuserdata(L, sizeof(lcu_ThreadPool *));
	*poolref = pool;
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, collectthreadpool);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);

	uv_mutex_lock(&pool->mutex);
	pool->tasks++;
	if (pool->idle > 0) {
		uv_cond_signal(&pool->onwork);
	} else if (pool->threads < pool->size) {
		uv_thread_t tid;
		err = uv_thread_create(&tid, runthread, pool);
		if (err) goto end;
		pool->threads++;
	}
	enqueuestateq(&pool->queue, L);
	pool->pending++;

	end:
	uv_mutex_unlock(&pool->mutex);

	return err;
}

LCULIB_API int lcu_countthreads (lcu_ThreadPool *pool,
                                 lcu_ThreadCount *count,
                                 const char *what) {
	uv_mutex_lock(&pool->mutex);
	for (; *what; ++what) switch (*what) {
		case 'e': count->expected = pool->size; break;
		case 'a': count->actual = pool->threads; break;
		case 'r': count->running = pool->running; break;
		case 'p': count->pending = pool->pending; break;
		case 's': count->suspended = pool->tasks-pool->running-pool->pending; break;
		case 'n': count->numoftasks = pool->tasks; break;
	}
	uv_mutex_unlock(&pool->mutex);
	return 0;
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

/*
 * Sync Ports
 */

#define LCU_SYNCPORTANY	(LCU_SYNCPORTIN|LCU_SYNCPORTOUT)

struct lcu_SyncPort {
	lua_Alloc allocf;
	void *allocud;
	uv_mutex_t mutex;
	int refcount;
	int expected;
	StateQ queue;
};

LCULIB_API lcu_SyncPort *lcu_newsyncport (lua_State *L) {
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	lcu_SyncPort *port = (lcu_SyncPort *)allocf(allocud, NULL, 0, sizeof(lcu_SyncPort));
	if (port == NULL) luaL_error(L, "not enough memory");

	port->allocf = allocf;
	port->allocud = allocud;
	uv_mutex_init(&port->mutex);
	port->refcount = 0;
	port->expected = 0;
	initstateq(&port->queue);

	lcu_pushsyncport(L, port);

	return port;
}

LCULIB_API void lcu_refsyncport (lcu_SyncPort *port) {
	uv_mutex_lock(&port->mutex);
	port->refcount++;
	uv_mutex_unlock(&port->mutex);
}

LCULIB_API void lcu_unrefsyncport (lcu_SyncPort *port) {
	int refcount;
	uv_mutex_lock(&port->mutex);
	refcount = port->refcount--;
	uv_mutex_unlock(&port->mutex);
	if (refcount == 0) {
		lua_State *L;
		while ((L = dequeuestateq(&port->queue))) lua_close(L);
		uv_mutex_destroy(&port->mutex);
		port->allocf(port->allocud, port, sizeof(lcu_SyncPort), 0);
	}
}

LCULIB_API void lcu_pushsyncport (lua_State *L, lcu_SyncPort *port) {
	lcu_SyncPort **ref;

	if (!luaL_getsubtable(L, LUA_REGISTRYINDEX, LCU_SYNCPORTREGISTRY)) {
		lua_newtable(L);
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
	} else {
		lua_pushlightuserdata(L, port);
		lua_gettable(L, -2);
		if (luaL_testudata(L, -1, LCU_SYNCPORTCLS)) goto found;
		lua_pop(L, 1);
	}

	ref = (lcu_SyncPort **)lua_newuserdata(L, sizeof(lcu_SyncPort *));
	*ref = NULL;  /* in case of errors */
	luaL_setmetatable(L, LCU_SYNCPORTCLS);
	lua_pushlightuserdata(L, port);
	lua_pushvalue(L, -2);
	lua_settable(L, -4);

	*ref = port;
	lcu_refsyncport(port);

	found:
	lua_remove(L, -2);
}

LCULIB_API int lcu_closesyncport (lua_State *L, int idx) {
	lcu_SyncPort **ref = (lcu_SyncPort **)luaL_checkudata(L, idx, LCU_SYNCPORTCLS);
	if (*ref) {
		lcu_unrefsyncport(*ref);
		*ref = NULL;
		return 1;
	}
	return 0;
}

LCULIB_API lcu_SyncPort *lcu_checksyncport (lua_State *L, int idx) {
	lcu_SyncPort **ref = (lcu_SyncPort **)luaL_checkudata(L, idx, LCU_SYNCPORTCLS);
	luaL_argcheck(L, *ref, idx, "closed synchronization port");
	return *ref;
}

static int auxpusherrfrom (lua_State *L) {
	lua_State *failed = (lua_State *)lua_touserdata(L, 1);
	const char *msg = lua_tostring(failed, -1);
	lua_pushfstring(L, "remote: %s", msg);
	lua_concat(L, 2);
	return 1;
}

static void pusherrfrom (lua_State *L, lua_State *failed) {
	lua_replace(failed, 1);
	lua_settop(failed, 1);
	lua_pushboolean(failed, 0);
	lua_replace(failed, 1);

	lua_settop(L, 0);
	lua_pushboolean(L, 0);
	lua_pushcfunction(L, auxpusherrfrom);
	lua_pushlightuserdata(L, failed);
	lua_pcall(L, 1, 1, 0);
}

LCULIB_API int lcu_pushsyncportfrom (lua_State *to, lua_State *from, int arg) {
	lcu_SyncPort **portref = lcu_tosyncportref(from, arg);
	if (portref && *portref) {
		lcu_pushsyncport(to, *portref);
		return 1;
	}
	return 0;
}

static void swapvalues (lua_State *src, lua_State *dst) {
	int i, err;
	int nsrc = lua_gettop(src);
	int ndst = lua_gettop(dst);

	if (nsrc < ndst) {
		{ lua_State *L = dst; dst = src; src = L; }
		{ int n = ndst; ndst = nsrc; nsrc = n; }
	}

	for (i = 3; i <= ndst; ++i) {
		err = lcuL_pushfrom(src, dst, i, "receive argument", lcu_pushsyncportfrom);
		if (err == LUA_OK) {
			lua_replace(src, i-1);
		} else {
			pusherrfrom(dst, src);
			return;
		}
		err = lcuL_pushfrom(dst, src, i, "receive argument", lcu_pushsyncportfrom);
		if (err == LUA_OK) {
			lua_replace(dst, i-1);
		} else {
			pusherrfrom(src, dst);
			return;
		}
	}

	lua_settop(dst, ndst-1);
	err = lcuL_movefrom(dst, src, 1+nsrc-ndst, "receive argument", lcu_pushsyncportfrom);
	if (err == LUA_OK) lua_settop(src, ndst-1);
	else pusherrfrom(src, dst);
}

static void rescheduletask (lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
	lcu_ThreadPool *pool = *((lcu_ThreadPool **)lua_touserdata(L, -1));
	lua_pop(L, 1);
	lcu_addthreads(pool, L);
}

LCULIB_API int lcu_matchsyncport (lcu_SyncPort **portref,
                                  const char *endname,
                                  lua_State *L) {
	lcu_SyncPort *port = *portref;
	int endpoint;
	NULL;
	if (endname == NULL) endname = "";
	switch (*endname) {
		case 'i': endpoint = LCU_SYNCPORTIN; break;
		case 'o': endpoint = LCU_SYNCPORTOUT; break;
		default: endpoint = LCU_SYNCPORTANY; break;
	}
	*portref = NULL;  /* avoid unref by '__gc', see unref at the end */

	uv_mutex_lock(&port->mutex);
	if (port->queue.head == NULL) {
		port->expected = endpoint == LCU_SYNCPORTANY ? LCU_SYNCPORTANY : endpoint^LCU_SYNCPORTANY;
	} else if (port->expected&endpoint) {
		lua_State *match = dequeuestateq(&port->queue);
		uv_mutex_unlock(&port->mutex);
		*portref = port;  /* restore unref by '__gc' */
		portref = lcu_tosyncportref(match, 1);
		lcu_assert(portref && *portref);
		*portref = port;  /* restore unref for 'match' */
		lcu_refsyncport(port);
		swapvalues(L, match);
		rescheduletask(match);
		return 1;
	}
	enqueuestateq(&port->queue, L);
	uv_mutex_unlock(&port->mutex);
	lcu_unrefsyncport(port);  /* unref while suspended, might destroy it */
	return 0;
}

/******************************************************************************/

/* getmetatable(syncport).__gc(syncport) */
static int syncport_gc (lua_State *L) {
	lcu_closesyncport(L, 1);
	return 0;
}

/* closed = syncport:close() */
static int syncport_close (lua_State *L) {
	int closed = lcu_closesyncport(L, 1);
	lua_pushboolean(L, closed);
	return 1;
}

/* channel [, errmsg] = system.syncport() */
static int system_syncport (lua_State *L) {
	lcu_newsyncport(L);
	return 1;
}



/* getmetatable(pool).__gc(pool) */
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
	lcu_ThreadCount count;
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	const char *options = luaL_checkstring(L, 2);
	lcu_countthreads(pool, &count, options);
	lua_settop(L, 2);
	for (; *options; ++options) switch (*options) {
		case 'e': lua_pushinteger(L, count.expected); break;
		case 'a': lua_pushinteger(L, count.actual); break;
		case 'r': lua_pushinteger(L, count.running); break;
		case 'p': lua_pushinteger(L, count.pending); break;
		case 's': lua_pushinteger(L, count.suspended); break;
		case 'n': lua_pushinteger(L, count.numoftasks); break;
		default: {
			char msg[19] = "invalid option '?'";
			msg[16] = *options;
			return luaL_argerror(L, 2, msg);
		}
	}
	return lua_gettop(L)-2;
}

static int loadsyncportcls (lua_State *L) {
	int ltype = luaL_getmetatable(L, LCU_SYNCPORTCLS);
	lua_pop(L, 1);
	if (ltype == LUA_TNIL) {
		static const luaL_Reg syncportf[] = {
			{"__gc", syncport_gc},
			{"close", syncport_close},
			{NULL, NULL}
		};
		lcuM_newclass(L, LCU_SYNCPORTCLS);
		lcuM_setfuncs(L, syncportf, 0);
		lua_pop(L, 1);
	}
	return 0;
}

static int dochunk (lua_State *L,
                    lcu_ThreadPool *pool,
                    lua_State *NL,
                    int status,
                    int narg) {
	int top;
	if (status != LUA_OK) {  /* error (message is on top of the stack) */
		size_t len;
		const char *errmsg = lua_tolstring(NL, -1, &len);
		lua_pushnil(L);
		lua_pushlstring(L, errmsg, len);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	top = lua_gettop(L);
	status = lcuL_movefrom(NL, L, top > narg ? top-narg : 0,
	                       "transfer argument", lcu_pushsyncportfrom);
	if (status != LUA_OK) {
		const char *msg = lua_tostring(NL, -1);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, msg);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	lua_pushcfunction(NL, loadsyncportcls);
	status = lua_pcall(NL, 0, 0, 0);
	if (status != LUA_OK) {
		size_t len;
		const char *errmsg = lua_tolstring(NL, -1, &len);
		lua_pushnil(L);
		lua_pushlstring(L, errmsg, len);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	status = lcu_addthreads(pool, NL);
	if (status) {
		lua_close(NL);
		return lcuL_pusherrres(L, status);
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
	return dochunk(L, pool, NL, status, 4);
}

/* succ [, errmsg] = threads:dofile([path [, mode, ...]]) */
static int threads_dofile (lua_State *L) {
	lcu_ThreadPool *pool = lcu_checkthreads(L, 1);
	const char *fpath = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return dochunk(L, pool, NL, status, 3);
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
		pool = *((lcu_ThreadPool **)lua_touserdata(L, -1));
		lua_pop(L, 1);
		lcu_pushthreads(L, pool);
	}
	return 1;
}



/* getmetatable(channel).__gc(channel) */
static int channel_gc (lua_State *L) {
	return 0;
}

/* res [, errmsg] = channel:sync(endpoint, ...) */
static int channel_sync (lua_State *L) {
	return 0;
}

/* res [, errmsg] = channel:prove(endpoint) */
static int channel_probe (lua_State *L) {
	return 0;
}

/* channel [, errmsg] = system.channel() */
static int system_channel (lua_State *L) {
	return 0;
}



LCUI_FUNC void lcuM_addthreadc (lua_State *L) {
	static const luaL_Reg poolf[] = {
		{"__gc", threadpool_gc},
		{NULL, NULL}
	};
	static const luaL_Reg threadsf[] = {
		{"close", threads_close},
		{"resize", threads_resize},
		{"count", threads_count},
		{"dostring", threads_dostring},
		{"dofile", threads_dofile},
		{NULL, NULL}
	};
	static const luaL_Reg channelf[] = {
		{"__gc", channel_gc},
		{"close", channel_gc},
		{"sync", channel_sync},
		{"probe", channel_probe},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_THREADPOOLCLS);
	lcuM_setfuncs(L, poolf, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, LCU_THREADSCLS);
	lcuM_setfuncs(L, threadsf, 0);
	lua_pop(L, 1);
	loadsyncportcls(L);
	lcuM_newclass(L, LCU_CHANNELCLS);
	lcuM_setfuncs(L, channelf, LCU_MODUPVS);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addthreadf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"threads", system_threads},
		{"syncport", system_syncport},
		{"channel", system_channel},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
}
