#include "lmodaux.h"
#include "loperaux.h"



#include <lmemlib.h>
#include <string.h>



#define LCU_THREADPOOLREGKEY	LCU_PREFIX"ThreadPool *parentpool"
#define LCU_THREADSCTL	LCU_PREFIX"ThreadPool"
#define LCU_THREADSCLS	LCU_PREFIX"threads"

#define LCU_CHANNELCTLREGKEY LCU_PREFIX"ChannelControl *channelctl"
#define LCU_CHANNELSYNCREGKEY LCU_PREFIX"uv_async_t *async"
#define LCU_CHANNELCTL LCU_PREFIX"channelctl"
#define LCU_CHANNELCLS LCU_PREFIX"channel"

#define LCU_ENDPOINTIN	0x01
#define LCU_ENDPOINTOUT	0x02
#define LCU_ENDPOINTBOTH	(LCU_ENDPOINTIN|LCU_ENDPOINTOUT)

#define LCU_NEXTSTATEREGKEY LCU_PREFIX"lua_State *nexttask"



typedef struct StateQ {
	lua_State *head;
	lua_State *tail;
} StateQ;

static void initstateq (StateQ *q) {
	q->head = NULL;
	q->tail = NULL;
}

static int emptystateq (StateQ *q) {
	return q->head == NULL && q->tail == NULL;
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



static int auxpusherrfrom (lua_State *L) {
	lua_State *failed = (lua_State *)lua_touserdata(L, 1);
	const char *msg = lua_tostring(failed, -1);
	if (strncmp(msg, "unable to receive argument #", 28) == 0) {
		lua_pushfstring(L, "unable to send argument %s", msg+27);
	} else {
		lua_pushstring(L, msg);
	}
	return 1;
}

static void pusherrfrom (lua_State *L, lua_State *failed) {
	lua_replace(failed, 1);
	lua_settop(failed, 1);
	lua_pushboolean(failed, 0);
	lua_insert(failed, 1);

	lua_settop(L, 0);
	lua_pushboolean(L, 0);
	lua_pushcfunction(L, auxpusherrfrom);
	lua_pushlightuserdata(L, failed);
	lua_pcall(L, 1, 1, 0);
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
		err = lcuL_pushfrom(src, dst, i, "argument");
		if (err == LUA_OK) {
			lua_replace(src, i-1);
		} else {
			pusherrfrom(dst, src);
			return;
		}
		err = lcuL_pushfrom(dst, src, i, "argument");
		if (err == LUA_OK) {
			lua_replace(dst, i-1);
		} else {
			pusherrfrom(src, dst);
			return;
		}
	}

	lua_settop(dst, ndst-1);
	err = lcuL_movefrom(dst, src, nsrc-ndst, "argument");
	if (err != LUA_OK) pusherrfrom(src, dst);
	else {
		lua_settop(src, ndst-1);
		lua_pushboolean(src, 1);
		lua_replace(src, 1);
		lua_pushboolean(dst, 1);
		lua_replace(dst, 1);
	}
}



typedef struct ChannelMap ChannelMap;

typedef struct ChannelSync ChannelSync;

typedef struct ChannelControl {
	uv_mutex_t mutex;
	int pending;
	lua_State *L;
} ChannelControl;

typedef struct ThreadPool {
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
	ChannelMap *channels;
} ThreadPool;

static int hasextraidle_mx (ThreadPool *pool) {
	return pool->threads > pool->size && pool->idle > 0;
}

static void checkhalted_mx (ThreadPool *pool) {
	if (pool->tasks == 0 && pool->size > 0) {
		pool->size = 0;
		uv_cond_signal(&pool->onwork);
	}
}

static void threadmain (void *arg);

static int addthread_mx (ThreadPool *pool, lua_State *L) {
	/* starting or waking threads won't get this new task */
	if (pool->threads-pool->running-pool->idle <= pool->pending) {
		if (pool->idle > 0) {
			pool->idle--;
			uv_cond_signal(&pool->onwork);
		} else if (pool->threads < pool->size) {
			uv_thread_t tid;
			int err = uv_thread_create(&tid, threadmain, pool);
			if (err) return err;
			pool->threads++;
		}
	}
	enqueuestateq(&pool->queue, L);
	pool->pending++;
	return 0;
}

static void rescheduletask (lua_State *L) {
	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELCTLREGKEY) == LUA_TLIGHTUSERDATA) {
		uv_async_t *async;
		ChannelControl *channelctl = (ChannelControl *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
		async = (uv_async_t *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		uv_mutex_lock(&channelctl->mutex);
		channelctl->pending++;
		L = channelctl->L;
		channelctl->L = NULL;
		uv_mutex_unlock(&channelctl->mutex);
		uv_async_send(async);
		if (L == NULL) return;
	} else {
		lua_pop(L, 1);
	}
	lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
	ThreadPool *pool = *((ThreadPool **)lua_touserdata(L, -1));
	lua_pop(L, 1);
	uv_mutex_lock(&pool->mutex);
	addthread_mx(pool, L);
	uv_mutex_unlock(&pool->mutex);
}



typedef struct ChannelSync {
	uv_mutex_t mutex;
	int refcount;
	int expected;
	StateQ queue;
} ChannelSync;

typedef lua_State *(*GetAsyncState) (lua_State *L, void *userdata);

static int channelsync_match (ChannelSync *sync,
                              const char *endname,
                              lua_State *L,
                              GetAsyncState getstate,
                              void *userdata) {
	int endpoint;
	switch (*endname) {
		case 'i': endpoint = LCU_ENDPOINTIN; break;
		case 'o': endpoint = LCU_ENDPOINTOUT; break;
		default: endpoint = LCU_ENDPOINTBOTH; break;
	}

	uv_mutex_lock(&sync->mutex);
	if (sync->queue.head == NULL) {
		sync->expected = endpoint == LCU_ENDPOINTBOTH ? LCU_ENDPOINTBOTH
		                                              : endpoint^LCU_ENDPOINTBOTH;
	} else if (sync->expected&endpoint) {
		lua_State *match = dequeuestateq(&sync->queue);
		uv_mutex_unlock(&sync->mutex);
		swapvalues(L, match);
		rescheduletask(match);
		return 1;
	}
	if (getstate != NULL) {
		L = getstate(L, userdata);
		if (L == NULL) goto match_end;
	}
	enqueuestateq(&sync->queue, L);

	match_end:
	uv_mutex_unlock(&sync->mutex);
	return L == NULL;
}



struct ChannelMap {
	uv_mutex_t mutex;
	lua_State *L;
};

static int channelmap_gc (lua_State *L) {
	ChannelMap *map = (ChannelMap *)lua_touserdata(L, 1);
	if (map->L) {
		uv_mutex_destroy(&map->mutex);
		map->L = NULL;
	}
	return 0;
}

static ChannelMap *channelmap_get (lua_State *L) {
	int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELMAP);
	ChannelMap *map = (ChannelMap *)lua_touserdata(L, -1);
	lcu_assert(type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA);
	lua_pop(L, 1);
	return map;
}

static ChannelSync *channelmap_getsync (ChannelMap *map, const char *name) {
	lua_State *L = map->L;
	ChannelSync *sync = NULL;
	uv_mutex_lock(&map->mutex);
	if (lua_getglobal(L, name) == LUA_TLIGHTUSERDATA) {
		sync = (ChannelSync *)lua_touserdata(L, -1);
		uv_mutex_lock(&sync->mutex);
		sync->refcount++;
		uv_mutex_unlock(&sync->mutex);
	} else {
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		sync = (ChannelSync *)allocf(allocud, NULL, 0, sizeof(ChannelSync));
		if (sync) {
			uv_mutex_init_recursive(&sync->mutex);
			sync->refcount = 1;
			sync->expected = 0;
			initstateq(&sync->queue);;
			lua_pushlightuserdata(L, sync);
			lua_setglobal(L, name);
		}
	}
	lua_pop(L, 1);
	uv_mutex_unlock(&map->mutex);
	return sync;
}

static void channelmap_freesync (ChannelMap *map, const char *name) {
	lua_State *L = map->L;
	ChannelSync *sync = NULL;
	uv_mutex_lock(&map->mutex);
	if (lua_getglobal(L, name) == LUA_TLIGHTUSERDATA) {
		sync = (ChannelSync *)lua_touserdata(L, -1);
		if (--sync->refcount == 0 && emptystateq(&sync->queue)) {
			void *allocud;
			lua_Alloc allocf = lua_getallocf(L, &allocud);
			uv_mutex_destroy(&sync->mutex);
			allocf(allocud, sync, sizeof(ChannelSync), 0);
			lua_pushnil(L);
			lua_setglobal(L, name);
		}
	}
	lua_pop(L, 1);
	uv_mutex_unlock(&map->mutex);
}



static void threadmain (void *arg) {
	ThreadPool *pool = (ThreadPool *)arg;
	lua_State *L = NULL;

	uv_mutex_lock(&pool->mutex);
	while (1) {
		int narg, status;
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
			}
		}
		pool->running++;
		uv_mutex_unlock(&pool->mutex);
		narg = lua_gettop(L);
		if (lua_status(L) == LUA_OK) narg--;
		status = lua_resume(L, NULL, narg);
		if (status == LUA_YIELD) {
			const char *channelname = lua_tostring(L, 1);
			if (channelname) {
				ChannelSync *sync = channelmap_getsync(pool->channels, channelname);
				const char *endname = luaL_opt(L, lua_tostring, 2, "");
				int narg = lua_gettop(L);
				if (narg < 2) {
					lua_settop(L, 2);
				} else if (!lcuL_canmove(L, narg-2, "argument")) {
					narg = 0;
				}
				if (narg && !channelsync_match(sync, endname, L, NULL, NULL)) L = NULL;
				channelmap_freesync(pool->channels, channelname);
			} else {
				lua_settop(L, 0);  /* discard returned values */
			}
		} else {
			if (status != LUA_OK) {
				lua_writestringerror("[COUTIL PANIC] %s\n", lua_tostring(L, -1));
			}
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
		pool->idle--;
		uv_cond_signal(&pool->onwork);
	} else if (pool->detached) {
		if (pool->threads == 0) uv_cond_signal(&pool->onterm);
		else checkhalted_mx(pool);
	}
	uv_mutex_unlock(&pool->mutex);
}

static int tpool_init (ThreadPool *pool, ChannelMap *channels) {
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
	pool->channels = channels;
	return 0;

	ontermcond_err:
	uv_cond_destroy(&pool->onwork);
	onworkcond_err:
	uv_mutex_destroy(&pool->mutex);
	mutex_err:
	return err;
}

static void tpool_close (ThreadPool *pool) {
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

static int tpool_resize (ThreadPool *pool, int size, int create) {
	int newthreads, err = 0;

	uv_mutex_lock(&pool->mutex);
	newthreads = size-pool->size;
	pool->size = size;
	if (newthreads > 0) {
		if (!create && newthreads > pool->pending) newthreads = pool->pending;
		while (newthreads--) {
			uv_thread_t tid;
			err = uv_thread_create(&tid, threadmain, pool);
			if (err) break;
			pool->threads++;
		}
	} else if (hasextraidle_mx(pool)) {
		pool->idle--;
		uv_cond_signal(&pool->onwork);
	}
	uv_mutex_unlock(&pool->mutex);

	return err;
}

static int collectthreadpool (lua_State *L) {
	ThreadPool *pool = *((ThreadPool **)lua_touserdata(L, 1));
	uv_mutex_lock(&pool->mutex);
	pool->tasks--;
	uv_mutex_unlock(&pool->mutex);
	return 0;
}

static int tpool_addtask (ThreadPool *pool, lua_State *L) {
	ThreadPool **poolref;
	int err;
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	poolref = (ThreadPool **)lua_newuserdata(L, sizeof(ThreadPool *));
	*poolref = pool;
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, collectthreadpool);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);

	uv_mutex_lock(&pool->mutex);
	pool->tasks++;
	err = addthread_mx(pool, L);
	uv_mutex_unlock(&pool->mutex);

	return err;
}

typedef struct ThreadCount {
	int expected;
	int actual;
	int running;
	int pending;
	int suspended;
	int numoftasks;
} ThreadCount;

static int tpool_count (ThreadPool *pool,
                        ThreadCount *count,
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



static ThreadPool *tothreads (lua_State *L, int idx) {
	ThreadPool **ref = (ThreadPool **)luaL_checkudata(L, idx, LCU_THREADSCLS);
	luaL_argcheck(L, *ref, idx, "closed threads");
	return *ref;
}

/* getmetatable(pool).__gc(pool) */
static int threadpool_gc (lua_State *L) {
	ThreadPool *pool = (ThreadPool *)luaL_checkudata(L, 1, LCU_THREADSCTL);
	tpool_close(pool);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	ThreadPool **ref = (ThreadPool **)luaL_checkudata(L, 1, LCU_THREADSCLS);
	lua_pushboolean(L, *ref != NULL);
	if (*ref) {
		lua_pushlightuserdata(L, *ref);
		if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TUSERDATA) {
			lcu_assert(*ref == lua_touserdata(L, -1));
			tpool_close(*ref);
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
	ThreadPool *pool = tothreads(L, 1);
	int size = (int)luaL_checkinteger(L, 2);
	int create = lua_toboolean(L, 3);
	luaL_argcheck(L, size >= 0, 2, "size cannot be negative");
	err = tpool_resize(pool, size, create);
	if (err) return lcuL_pusherrres(L, err);
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:count([option]) */
static int threads_count (lua_State *L) {
	ThreadCount count;
	ThreadPool *pool = tothreads(L, 1);
	const char *opt = luaL_checkstring(L, 2);
	tpool_count(pool, &count, opt);
	lua_settop(L, 2);
	for (; *opt; ++opt) switch (*opt) {
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

static int dochunk (lua_State *L,
                    ThreadPool *pool,
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
	status = lcuL_movefrom(NL, L, top > narg ? top-narg : 0, "argument");
	if (status != LUA_OK) {
		const char *msg = lua_tostring(NL, -1);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, msg);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	status = tpool_addtask(pool, NL);
	if (status) {
		lua_close(NL);
		return lcuL_pusherrres(L, status);
	}
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:dostring(chunk [, chunkname [, mode, ...]]) */
static int threads_dostring (lua_State *L) {
	ThreadPool *pool = tothreads(L, 1);
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
	ThreadPool *pool = tothreads(L, 1);
	const char *fpath = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, NULL);
	lua_State *NL = lcuL_newstate(L);  /* create a similar state */
	int status = luaL_loadfilex(NL, fpath, mode);
	return dochunk(L, pool, NL, status, 3);
}

/* threads [, errmsg] = system.threads([size]) */
static int system_threads (lua_State *L) {
	ThreadPool *pool;
	if (lua_gettop(L) > 0) {
		int err, size = (int)luaL_checkinteger(L, 1);
		ChannelMap *map = channelmap_get(L);
		pool = (ThreadPool *)lua_newuserdata(L, sizeof(ThreadPool));
		err = tpool_init(pool, map);
		if (err) return lcuL_pusherrres(L, err);
		if (size > 0) {
			err = tpool_resize(pool, size, 1);
			if (err) {
				tpool_close(pool);
				return lcuL_pusherrres(L, err);
			}
		}
		luaL_setmetatable(L, LCU_THREADSCTL);
		lua_pushlightuserdata(L, pool);
		lua_insert(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	} else {
		int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_THREADPOOLREGKEY);
		if (type == LUA_TNIL) return 1;
		pool = *((ThreadPool **)lua_touserdata(L, -1));
		lua_pop(L, 1);
	}
	{
		ThreadPool **ref = (ThreadPool **)lua_newuserdata(L, sizeof(ThreadPool*));
		*ref = pool;
		luaL_setmetatable(L, LCU_THREADSCLS);
	}
	return 1;
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
	lcuM_newclass(L, LCU_THREADSCTL);
	lcuM_setfuncs(L, poolf, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, LCU_THREADSCLS);
	lcuM_setfuncs(L, threadsf, 0);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addthreadf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"threads", system_threads},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
}



typedef struct LuaChannel {
	ChannelSync *sync;
	lua_State *L;
} LuaChannel;

#define tolchannel(L,I)	((LuaChannel *)luaL_checkudata(L,I,LCU_CHANNELCLS))

static LuaChannel *chklchannel(lua_State *L, int arg) {
	LuaChannel *channel = (LuaChannel *)luaL_checkudata(L, arg, LCU_CHANNELCLS);
	int ltype = lua_getuservalue(L, arg);
	luaL_argcheck(L, ltype == LUA_TSTRING, arg, "closed channel");
	lua_pop(L, 1);
	return channel;
}

static int channelclose (lua_State *L) {
	ChannelMap *map = channelmap_get(L);
	LuaChannel *channel = tolchannel(L, 1);
	if (lua_getuservalue(L, 1) == LUA_TSTRING) {
		const char *name = lua_tostring(L, -1);
		channelmap_freesync(map, name);
		lua_close(channel->L);
		/* DEBUG: channel->sync = NULL; */
		/* DEBUG: channel->L = NULL; */
		lua_pushnil(L);
		lua_setuservalue(L, 1);
		return 1;
	}
	return 0;
}

static int channelsync (LuaChannel *channel,
                        lua_State *L,
                        GetAsyncState getstate,
                        void *userdata) {
	const char *endname = luaL_optstring(L, 2, "");
	int narg = lua_gettop(L);
	if (narg < 2) {
		lua_settop(L, 2);
	} else if (!lcuL_canmove(L, narg-2, "argument")) {
		lua_error(L);
	}
	return channelsync_match(channel->sync, endname, L, getstate, userdata);
}

/* getmetatable(channel).__gc(channel) */
static int channel_gc (lua_State *L) {
	channelclose(L);
	return 0;
}

/* getmetatable(channel).__gc(channel) */
static int channel_close (lua_State *L) {
	int closed = channelclose(L);
	lua_pushboolean(L, closed);
	return 1;
}

/* res [, errmsg] = channel:await(endpoint, ...) */
static int returnsynced (lua_State *L) {
	LuaChannel *channel = chklchannel(L, 1);
	int nret = lua_gettop(channel->L);
	int err = lcuL_movefrom(L, channel->L, nret, "");
	lcu_assert(err == LUA_OK);
	lua_pushnil(channel->L);
	lua_setfield(channel->L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
	return nret;
}

static void uv_onsynced (uv_async_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	lcuU_resumethrop(thread, (uv_handle_t *)handle);
}

typedef struct ArmSyncedArgs {
	uv_loop_t *loop;
	uv_async_t *async;
	lua_State *L;
} ArmSyncedArgs;

static lua_State *armsynced (lua_State *L, void *data) {
	ArmSyncedArgs *args = (ArmSyncedArgs *)data;
	int err;
	lcu_assert(lua_gettop(args->L) == 0);
	lua_pushlightuserdata(args->L, args->async);
	lua_setfield(args->L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
	lua_settop(args->L, 2);  /* placeholder for 'syncport' and 'endname' */
	err = lcuL_movefrom(args->L, L, lua_gettop(L)-2, "argument");
	if (err != LUA_OK) {
		const char *msg = lua_tostring(args->L, -1);
		lua_settop(L, 0);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, msg);
		lua_settop(args->L, 0);
		return NULL;
	}
	err = lcuT_armthrop(L, uv_async_init(args->loop, args->async, uv_onsynced));
	if (err < 0) {
		lua_settop(L, 0);
		lcuL_pusherrres(L, err);
		lua_settop(args->L, 0);
		return NULL;
	}
	return args->L;
}

static int k_setupsynced (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	LuaChannel *channel = chklchannel(L, 1);
	ArmSyncedArgs args;
	args.loop = loop;
	args.async = (uv_async_t *)handle;
	args.L = channel->L;
	if (channelsync(channel, L, armsynced, &args)) return lua_gettop(L);
	return -1;
}

static int channel_await (lua_State *L) {
	return lcuT_resetthropk(L, -1, k_setupsynced, returnsynced);
}

/* res [, errmsg] = channel:sync(endpoint) */
static lua_State *cancelsuspension (lua_State *L, void *data) {
	lua_settop(L, 0);
	lua_pushnil(L);
	lua_pushliteral(L, "empty");
	return NULL;
}

static int channel_sync (lua_State *L) {
	LuaChannel *channel = chklchannel(L, 1);
	channelsync(channel, L, cancelsuspension, NULL);
	return lua_gettop(L);
}

static int channel_getname (lua_State *L) {
	chklchannel(L, 1);
	lua_getuservalue(L, 1);
	return 1;
}

static int channelctl_gc (lua_State *L) {
	ChannelControl *channelctl = (ChannelControl *)lua_touserdata(L, 1);
	uv_mutex_destroy(&channelctl->mutex);
	/* channelctl->pending = 0; */
	/* channelctl->L = NULL; */
	return 0;
}

/* channel [, errmsg] = system.channel(name) */
static int system_channel (lua_State *L) {
	ChannelMap *map = channelmap_get(L);
	const char *name = luaL_checkstring(L, 1);
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	LuaChannel *channel = (LuaChannel *)lua_newuserdata(L, sizeof(LuaChannel));
	ChannelControl *channelctl;

	lua_pushvalue(L, 1);
	lua_setuservalue(L, -2);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELCTLREGKEY) == LUA_TNIL) {
		channelctl = (ChannelControl *)lua_newuserdata(L, sizeof(ChannelControl));
		channelctl->pending = 0;
		channelctl->L = NULL;
		int err = uv_mutex_init(&channelctl->mutex);
		if (err) return lcuL_pusherrres(L, err);
		luaL_setmetatable(L, LCU_CHANNELCTL);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELCTLREGKEY);
	} else {
		channelctl = (ChannelControl *)lua_touserdata(L, -1);
	}
	lua_pop(L, 1);

	channel->L = lua_newstate(allocf, allocud);
	if (channel->L == NULL) luaL_error(L, "not enough memory");
	channel->sync = channelmap_getsync(map, name);
	if (channel->sync == NULL) {
		lua_close(channel->L);
		luaL_error(L, "not enough memory");
	}
	luaL_setmetatable(L, LCU_CHANNELCLS);

	lua_pushlightuserdata(channel->L, channelctl);
	lua_setfield(channel->L, LUA_REGISTRYINDEX, LCU_CHANNELCTLREGKEY);

	return 1;
}

/* names [, errmsg] = system.channelnames([names]) */
static int system_channelnames (lua_State *L) {
	ChannelMap *map = channelmap_get(L);
	lua_State *tabL = NULL;

	if (lua_isnoneornil(L, 1)) {
		lua_settop(L, 0);
		lua_newtable(L);
	} else {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_settop(L, 1);
		tabL = L;
	}
	uv_mutex_lock(&map->mutex);
	if (tabL == NULL) {
		lcu_assert(lua_gettop(map->L) == 0);
		lua_pushglobaltable(map->L);
		tabL = map->L;
	}
	lua_pushnil(tabL);
	while (lua_next(tabL, 1)) {
		const char *name = lua_tostring(tabL, -2);
		lua_pop(tabL, 1);
		if (name) {
			int type = lua_getglobal(map->L, name);
			if (type == LUA_TLIGHTUSERDATA) {
				lua_pushboolean(L, 1);
			} else {
				lua_pushnil(L);
			}
			lua_setfield(L, 1, name);
			lua_pop(map->L, 1);
		}
	}
	lua_settop(map->L, 0);  /* remove global table */
	uv_mutex_unlock(&map->mutex);
	return 1;
}



LCUI_FUNC void lcuM_addchanelc (lua_State *L) {
	static const luaL_Reg channelctlf[] = {
		{"__gc", channelctl_gc},
		{NULL, NULL}
	};
	static const luaL_Reg channelf[] = {
		{"__gc", channel_gc},
		{"close", channel_close},
		{"await", channel_await},
		{"sync", channel_sync},
		{"getname", channel_getname},
		{NULL, NULL}
	};
	lcuM_newclass(L, LCU_CHANNELCTL);
	lcuM_setfuncs(L, channelctlf, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, LCU_CHANNELCLS);
	lcuM_setfuncs(L, channelf, LCU_MODUPVS);
	lua_pop(L, 1);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELMAP) == LUA_TNIL) {
		ChannelMap *map = (ChannelMap *)lua_newuserdata(L, sizeof(ChannelMap));
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		map->L = NULL;
		lua_createtable(L, 0, 1);
		lua_pushcfunction(L, channelmap_gc);
		lua_setfield(L, -2, "__gc");
		lua_setmetatable(L, -2);
		map->L = lua_newstate(allocf, allocud);
		if (map->L == NULL) luaL_error(L, "not enough memory");
		uv_mutex_init(&map->mutex);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELMAP);
	}
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addchanelf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"channelnames", system_channelnames},
		{"channel", system_channel},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, 0);
}
