#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>
#include <string.h>



#define CLASS_TPOOLGC	LCU_PREFIX"ThreadPool *"
#define CLASS_THREADS	LCU_PREFIX"threads"
#define CLASS_CHANNELTASK	LCU_PREFIX"ChannelTask"
#define CLASS_CHANNEL	LCU_PREFIX"channel"

#define REGKEY_CHANNELSYNC	LCU_PREFIX"uv_async_t channelWake"
#define REGKEY_NEXTSTATE	LCU_PREFIX"lua_State nextQueuedTask"

#define ENDPOINT_IN	0x01
#define ENDPOINT_OUT	0x02
#define ENDPOINT_BOTH	(ENDPOINT_IN|ENDPOINT_OUT)



typedef struct StateQ {
	lua_State *head;
	lua_State *tail;
} StateQ;

static lua_State *getnextstateq (lua_State *L) {
	lua_State *next;
	lcu_assert(lua_checkstack(L, 1));
	lua_getfield(L, LUA_REGISTRYINDEX, REGKEY_NEXTSTATE);
	next = (lua_State *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return next;
}

static void setnextstateq (lua_State *L, lua_State *value) {
	if (value) lua_pushlightuserdata(L, value);
	else lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, REGKEY_NEXTSTATE);
}

static void initstateq (StateQ *q) {
	q->head = NULL;
	q->tail = NULL;
}

static int emptystateq (StateQ *q) {
	return q->head == NULL;
}

static void appendstateq (StateQ *q, StateQ *q2) {
	if (q->head) {
		lcu_assert(lua_checkstack(q->tail, 1));
		setnextstateq(q->tail, q2->head);
	} else {
		q->head = q2->head;
	}
	q->tail = q2->tail;
}

static void enqueuestateq (StateQ *q, lua_State *L) {
	StateQ single = { L, L };
	appendstateq(q, &single);
}

static lua_State *dequeuestateq (StateQ *q) {
	lua_State *L = q->head;
	if (L) {
		if (L == q->tail) {
			q->head = NULL;
			q->tail = NULL;
		} else {
			q->head = getnextstateq(L);
		}
		setnextstateq(L, NULL);
	}
	return L;
}

static lua_State *removestateq (StateQ *q, lua_State *L) {
	lua_State *prev = q->head;
	if (prev != NULL) {
		if (L == prev) return dequeuestateq(q);
		while (prev != q->tail) {
			lua_State *current = getnextstateq(prev);
			if (current == L) {
				if (L == q->tail) {
					setnextstateq(prev, NULL);
					q->tail = prev;
				} else {
					current = getnextstateq(L);
					setnextstateq(prev, current);
				}
				setnextstateq(L, NULL);
				return L;
			}
			prev = current;
		}
	}
	return NULL;
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

typedef struct ChannelTask {
	uv_mutex_t mutex;
	int wakes;
	lua_State *L;
} ChannelTask;

typedef enum { TPOOL_OPEN, TPOOL_CLOSING, TPOOL_CLOSED } TPoolStatus;

typedef struct ThreadPool {
	lua_Alloc allocf;
	void *allocud;
	uv_mutex_t mutex;
	uv_cond_t onwork;
	uv_cond_t onterm;
	TPoolStatus status;
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
			if (!err) pool->threads++;
			// TODO: show warn in Lua 5.4
		}
	}
	if (pool->status == TPOOL_CLOSED) return 0;
	enqueuestateq(&pool->queue, L);
	pool->pending++;
	return 1;
}

static void rescheduletask (lua_State *L) {
	ThreadPool *pool;
	int added;
	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TLIGHTUSERDATA) {
		uv_async_t *async;
		ChannelTask *channeltask = (ChannelTask *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, LUA_REGISTRYINDEX, REGKEY_CHANNELSYNC);
		async = (uv_async_t *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		uv_mutex_lock(&channeltask->mutex);
		L = channeltask->L;
		channeltask->L = NULL;
		channeltask->wakes++;
		uv_mutex_unlock(&channeltask->mutex);
		uv_async_send(async);
		if (L == NULL) return;
	} else {
		lua_pop(L, 1);
	}
	lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
	pool = *((ThreadPool **)lua_touserdata(L, -1));
	lua_pop(L, 1);
	uv_mutex_lock(&pool->mutex);
	added = addthread_mx(pool, L);
	uv_mutex_unlock(&pool->mutex);
	if (!added) lua_close(L);
}



typedef struct ChannelSync {
	uv_mutex_t mutex;
	int refcount;
	int expected;
	StateQ queue;
} ChannelSync;

typedef lua_State *(*GetAsyncState) (lua_State *L, void *userdata);

static int channelsync_match (ChannelSync *sync,
                              int endpoint,
                              lua_State *L,
                              GetAsyncState getstate,
                              void *userdata) {
	uv_mutex_lock(&sync->mutex);
	if (sync->queue.head == NULL) {
		sync->expected = endpoint == ENDPOINT_BOTH ? ENDPOINT_BOTH
		                                           : endpoint^ENDPOINT_BOTH;
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
	StateQ queue;
	initstateq(&queue);

	uv_mutex_lock(&map->mutex);
	L = map->L;
	lcu_assert(L);
	lcu_assert(lua_gettop(L) == 0);
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L, 1)) {
		const char *name = lua_tostring(L, -2);
		if (name) {
			ChannelSync *sync = (ChannelSync *)lua_touserdata(L, -1);
			uv_mutex_lock(&sync->mutex);
			if (!emptystateq(&sync->queue)) {
				appendstateq(&queue, &sync->queue);
				initstateq(&sync->queue);
			}
			uv_mutex_unlock(&sync->mutex);
		}
		lua_pop(L, 1);
	}
	lua_settop(L, 0);  /* remove global table */
	uv_mutex_unlock(&map->mutex);

	while ((L = dequeuestateq(&queue))) lua_close(L);

	uv_mutex_destroy(&map->mutex);
	lua_close(map->L);
	map->L = NULL;

	return 0;
}

static ChannelMap *channelmap_get (lua_State *L) {
	int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
	ChannelMap *map = (ChannelMap *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	lcu_assert(type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA);
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



static int checksyncargs (lua_State *L) {
	static const char *const options[] = { "in", "out", "any" };
	static const int endpoints[] = { ENDPOINT_IN, ENDPOINT_OUT, ENDPOINT_BOTH };
	const char *name = luaL_optstring(L, 2, "any");
	int i;
	for (i = 0; options[i]; i++) {
		if (strcmp(options[i], name) == 0) {
			int narg = lua_gettop(L);
			if (narg < 2) {
				lua_settop(L, 2);
			} else if (!lcuL_canmove(L, narg-2, "argument")) {
				return -1;
			}
			return endpoints[i];
		}
	}
	lua_settop(L, 0);
	lua_pushnil(L);
	lua_pushfstring(L, "bad argument #2 (invalid option '%s')", name);
	return -1;
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
			} else if (pool->status == TPOOL_CLOSING && pool->tasks == 0) {  /* if halted? */
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
				int endpoint = checksyncargs(L);
				if (endpoint != -1 && !channelsync_match(sync, endpoint, L, NULL, NULL))
					L = NULL;
				channelmap_freesync(pool->channels, channelname);
			} else {
				ChannelTask *channeltask = (ChannelTask *)luaL_testudata(L, 1, CLASS_CHANNELTASK);
				if (channeltask != NULL) {
					uv_mutex_lock(&channeltask->mutex);
					if (channeltask->wakes == 0) {
						channeltask->L = L;
						L = NULL;
					}
					uv_mutex_unlock(&channeltask->mutex);
				} else {
					lua_settop(L, 0);  /* discard returned values */
				}
			}
		} else {
			if (status != LUA_OK) {
				lua_writestringerror("[COUTIL PANIC] %s\n", lua_tostring(L, -1));
			}
			/* avoid 'pool->tasks--' */
			lua_settop(L, 0);
			lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
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
	} else if (pool->status == TPOOL_CLOSING) {
		if (pool->threads == 0) uv_cond_signal(&pool->onterm);
		else checkhalted_mx(pool);
	}
	uv_mutex_unlock(&pool->mutex);
}

static int tpool_create (ThreadPool **ref,
                         lua_Alloc allocf,
                         void *allocud,
                         ChannelMap *channels) {
	int err = UV_ENOMEM;
	ThreadPool *pool = (ThreadPool *)allocf(allocud, NULL, 0, sizeof(ThreadPool));
	if (pool == NULL) goto alloc_err;
	err = uv_mutex_init(&pool->mutex);
	if (err) goto mutex_err;
	err = uv_cond_init(&pool->onwork);
	if (err) goto onworkcond_err;
	err = uv_cond_init(&pool->onterm);
	if (err) goto ontermcond_err;

	pool->allocf = allocf;
	pool->allocud = allocud;
	pool->status = TPOOL_OPEN;
	pool->size = 0;
	pool->threads = 0;
	pool->idle = 0;
	pool->tasks = 0;
	pool->running = 0;
	pool->pending = 0;
	initstateq(&pool->queue);
	pool->channels = channels;
	*ref = pool;
	return 0;

	ontermcond_err:
	uv_cond_destroy(&pool->onwork);
	onworkcond_err:
	uv_mutex_destroy(&pool->mutex);
	mutex_err:
	allocf(allocud, pool, sizeof(ThreadPool), 0);
	alloc_err:
	return err;
}

static void tpool_destroy (ThreadPool *pool) {
	uv_mutex_destroy(&pool->mutex);
	pool->allocf(pool->allocud, pool, sizeof(ThreadPool), 0);
}

static void tpool_close (ThreadPool *pool) {
	int tasks, pending;
	StateQ queue;

	uv_mutex_lock(&pool->mutex);
	pool->status = TPOOL_CLOSING;
	if (pool->threads > 0) {
		checkhalted_mx(pool);
		uv_cond_wait(&pool->onterm, &pool->mutex);
	}
	tasks = pool->tasks;
	pending = pool->pending;
	queue = pool->queue;
	pool->pending = 0;
	initstateq(&pool->queue);
	pool->status = TPOOL_CLOSED;
	uv_mutex_unlock(&pool->mutex);

	uv_cond_destroy(&pool->onterm);
	uv_cond_destroy(&pool->onwork);

	if (tasks == 0) {
		tpool_destroy(pool);
	} else if (pending > 0) {
		lua_State *L;
		while ((L = dequeuestateq(&queue))) lua_close(L);
	}
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
	int status, tasks;
	uv_mutex_lock(&pool->mutex);
	status = pool->status;
	tasks = --pool->tasks;
	uv_mutex_unlock(&pool->mutex);
	if (status == TPOOL_CLOSED && tasks == 0) tpool_destroy(pool);
	return 0;
}

static int tpool_addtask (ThreadPool *pool, lua_State *L) {
	ThreadPool **poolref;
	int added;
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	poolref = (ThreadPool **)lua_newuserdata(L, sizeof(ThreadPool *));
	*poolref = pool;
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, collectthreadpool);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);

	uv_mutex_lock(&pool->mutex);
	added = addthread_mx(pool, L);
	lcu_assert(added);
	pool->tasks++;
	uv_mutex_unlock(&pool->mutex);

	return 0;
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
	ThreadPool **ref = (ThreadPool **)luaL_checkudata(L, idx, CLASS_THREADS);
	luaL_argcheck(L, *ref, idx, "closed threads");
	return *ref;
}

/* getmetatable(tpoolgc).__gc(pool) */
static int tpoolgc_gc (lua_State *L) {
	ThreadPool **ref = (ThreadPool **)luaL_checkudata(L, 1, CLASS_TPOOLGC);
	tpool_close(*ref);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	ThreadPool **ref = (ThreadPool **)luaL_checkudata(L, 1, CLASS_THREADS);
	lua_pushboolean(L, *ref != NULL);
	if (*ref) {
		lua_pushlightuserdata(L, *ref);
		if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TUSERDATA) {
			lcu_assert(*ref == *((ThreadPool **)lua_touserdata(L, -1)));
			/* remove userdata from registry */
			lua_pushlightuserdata(L, *ref);
			lua_pushnil(L);
			lua_settable(L, LUA_REGISTRYINDEX);
			/* disable userdata GC metamethod */
			lua_pushnil(L);
			lua_setmetatable(L, -2);
			/* destroy thread pool on userdata */
			tpool_close(*ref);
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
	return lcuL_pushresults(L, 0, err);
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
		ThreadPool **ref = (ThreadPool **)lua_newuserdata(L, sizeof(ThreadPool *));
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		err = tpool_create(ref, allocf, allocud, map);
		if (err) return lcuL_pusherrres(L, err);
		pool = *ref;
		if (size > 0) {
			err = tpool_resize(pool, size, 1);
			if (err) {
				tpool_close(pool);
				return lcuL_pusherrres(L, err);
			}
		}
		luaL_setmetatable(L, CLASS_TPOOLGC);
		lua_pushlightuserdata(L, pool);
		lua_insert(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	} else {
		int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
		if (type == LUA_TNIL) return 1;
		pool = *((ThreadPool **)lua_touserdata(L, -1));
		lua_pop(L, 1);
	}
	{
		ThreadPool **ref = (ThreadPool **)lua_newuserdata(L, sizeof(ThreadPool *));
		*ref = pool;
		luaL_setmetatable(L, CLASS_THREADS);
	}
	return 1;
}



LCUI_FUNC void lcuM_addthreadc (lua_State *L) {
	static const luaL_Reg poolreff[] = {
		{"__gc", tpoolgc_gc},
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
	lcuM_newclass(L, CLASS_TPOOLGC);
	lcuM_setfuncs(L, poolreff, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, CLASS_THREADS);
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
	uv_async_t *handle;
} LuaChannel;

#define tolchannel(L,I)	((LuaChannel *)luaL_checkudata(L,I,CLASS_CHANNEL))

static LuaChannel *chklchannel(lua_State *L, int arg) {
	LuaChannel *channel = tolchannel(L, arg);
	int ltype = lua_getuservalue(L, arg);
	luaL_argcheck(L, ltype == LUA_TSTRING, arg, "closed channel");
	lua_pop(L, 1);
	return channel;
}

static int channelclose (lua_State *L, LuaChannel *channel) {
	ChannelMap *map = channelmap_get(L);
	if (lua_getuservalue(L, 1) == LUA_TSTRING) {
		const char *name = lua_tostring(L, -1);
		if (channel->handle) {
			/* whole lua_State is closing, but still waiting on channel */
			lua_State *cL;
			ChannelSync *sync = channel->sync;
			uv_mutex_lock(&sync->mutex);
			cL = removestateq(&sync->queue, channel->L);
			uv_mutex_unlock(&sync->mutex);
			if (cL == NULL) {
				uv_loop_t *loop = channel->handle->loop;
				lcu_assert(loop->data == NULL);
				loop->data = (void *)L;
				do uv_run(loop, UV_RUN_ONCE);
				while (channel->handle);
				loop->data = NULL;
			}
		}
		lua_close(channel->L);
		channelmap_freesync(map, name);
		/* DEBUG: channel->sync = NULL; */
		/* DEBUG: channel->L = NULL; */
		/* DEBUG: channel->handle = NULL; */
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
	int endpoint = checksyncargs(L);
	if (endpoint == -1) lua_error(L);
	return channelsync_match(channel->sync, endpoint, L, getstate, userdata);
}

/* getmetatable(channel).__gc(channel) */
static int channel_gc (lua_State *L) {
	channelclose(L, tolchannel(L, 1));
	return 0;
}

/* channel:close() */
static int channel_close (lua_State *L) {
	LuaChannel *channel = tolchannel(L, 1);
	luaL_argcheck(L, channel->handle == NULL, 1, "in use");
	lua_pushboolean(L, channelclose(L, channel));
	return 1;
}

/* res [, errmsg] = channel:await(endpoint, ...) */
static void restorechannel (LuaChannel * channel) {
	lua_State *L = channel->L;
	lua_settop(L, 0);  /* discard arguments */
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, REGKEY_CHANNELSYNC);
	channel->handle = NULL;
}

static int returnsynced (lua_State *L) {
	LuaChannel *channel = (LuaChannel *)lua_touserdata(L, 1);
	lua_State *cL = channel->L;
	int nret = lua_gettop(cL);
	int err = lcuL_movefrom(L, cL, nret, "");
	lcu_assert(err == LUA_OK);
	restorechannel(channel);
	return nret;
}

static int cancelsynced (lua_State *L) {
	LuaChannel *channel = (LuaChannel *)lua_touserdata(L, 1);
	ChannelSync *sync = channel->sync;
	lua_State *cL = channel->L;
	uv_mutex_lock(&sync->mutex);
	cL = removestateq(&sync->queue, cL);
	uv_mutex_unlock(&sync->mutex);
	if (cL != NULL) {
		restorechannel(channel);
		return 1;
	}
	return 0;
}

static void uv_onsynced (uv_async_t *async) {
	uv_handle_t *handle = (uv_handle_t *)async;
	lua_State *thread = (lua_State *)handle->data;
	ChannelTask *channeltask;
	lua_getfield(thread, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
	channeltask = (ChannelTask *)lua_touserdata(thread, -1);
	lua_pop(thread, 1);
	uv_mutex_lock(&channeltask->mutex);
	channeltask->wakes--;
	uv_mutex_unlock(&channeltask->mutex);
	if (lcuU_endthrop(handle)) {
		lcuU_resumethrop(thread, handle);
	} else {
		LuaChannel *channel = (LuaChannel *)lua_touserdata(thread, 1);
		restorechannel(channel);
	}
	lcuU_checksuspend(handle->loop);
}

typedef struct ArmSyncedArgs {
	uv_loop_t *loop;
	uv_async_t *async;
	LuaChannel *channel;
} ArmSyncedArgs;

static lua_State *armsynced (lua_State *L, void *data) {
	ArmSyncedArgs *args = (ArmSyncedArgs *)data;
	lua_State *cL = args->channel->L;
	int err;
	lcu_assert(lua_gettop(cL) == 0);
	lua_pushlightuserdata(cL, args->async);
	lua_setfield(cL, LUA_REGISTRYINDEX, REGKEY_CHANNELSYNC);
	lua_settop(cL, 2);  /* placeholder for 'channel' and 'endname' */
	err = lcuL_movefrom(cL, L, lua_gettop(L)-2, "argument");
	if (err != LUA_OK) {
		const char *msg = lua_tostring(cL, -1);
		lua_settop(L, 0);
		lua_pushnil(L);
		lua_pushstring(L, msg);
		lua_settop(cL, 0);
		return NULL;
	}
	if (args->loop != NULL) {
		err = lcuT_armthrop(L, uv_async_init(args->loop, args->async, uv_onsynced));
		if (err < 0) {
			lua_settop(L, 0);
			lcuL_pusherrres(L, err);
			lua_settop(cL, 0);
			return NULL;
		}
	}

	args->channel->handle = args->async;

	return cL;
}

static int k_setupsynced (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	ArmSyncedArgs args;
	LuaChannel *channel = chklchannel(L, 1);
	luaL_argcheck(L, channel->handle == NULL, 1, "in use");
	args.loop = loop;
	args.async = (uv_async_t *)handle;
	args.channel = channel;
	if (channelsync(channel, L, armsynced, &args)) return lua_gettop(L);
	return -1;
}

static int channel_await (lua_State *L) {
	return lcuT_resetthropk(L, UV_ASYNC, k_setupsynced,
	                                     returnsynced,
	                                     cancelsynced);
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

static int channeltask_gc (lua_State *L) {
	ChannelTask *channeltask = (ChannelTask *)lua_touserdata(L, 1);
	uv_mutex_destroy(&channeltask->mutex);
	return 0;
}

/* channel [, errmsg] = system.channel(name) */
static int system_channel (lua_State *L) {
	ChannelMap *map = channelmap_get(L);
	const char *name = luaL_checkstring(L, 1);
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	LuaChannel *channel = (LuaChannel *)lua_newuserdata(L, sizeof(LuaChannel));
	ChannelTask *channeltask;

	lua_pushvalue(L, 1);
	lua_setuservalue(L, -2);

	channel->handle = NULL;
	channel->L = lua_newstate(allocf, allocud);
	if (channel->L == NULL) luaL_error(L, "not enough memory");
	channel->sync = channelmap_getsync(map, name);
	if (channel->sync == NULL) {
		lua_close(channel->L);
		luaL_error(L, "not enough memory");
	}
	luaL_setmetatable(L, CLASS_CHANNEL);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TNIL) {
		int err;
		channeltask = (ChannelTask *)lua_newuserdata(L, sizeof(ChannelTask));
		channeltask->wakes = 0;
		channeltask->L = NULL;
		err = uv_mutex_init(&channeltask->mutex);
		if (err) return lcuL_pusherrres(L, err);
		luaL_setmetatable(L, CLASS_CHANNELTASK);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
	} else {
		channeltask = (ChannelTask *)lua_touserdata(L, -1);
	}
	lua_pop(L, 1);

	lua_pushlightuserdata(channel->L, channeltask);
	lua_setfield(channel->L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);

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
	static const luaL_Reg channeltaskf[] = {
		{"__gc", channeltask_gc},
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
	lcuM_newclass(L, CLASS_CHANNELTASK);
	lcuM_setfuncs(L, channeltaskf, 0);
	lua_pop(L, 1);
	lcuM_newclass(L, CLASS_CHANNEL);
	lcuM_setfuncs(L, channelf, LCU_MODUPVS);
	lua_pop(L, 1);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY) == LUA_TNIL) {
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
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
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
