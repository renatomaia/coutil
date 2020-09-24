#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>
#include <string.h>


/*
 * lchsync.h
 */

typedef struct lcu_StateQ lcu_StateQ;


LCUI_FUNC void lcuCS_initstateq (lcu_StateQ *q);

LCUI_FUNC int lcuCS_emptystateq (lcu_StateQ *q);

LCUI_FUNC void lcuCS_enqueuestateq (lcu_StateQ *q, lua_State *L);

LCUI_FUNC lua_State *lcuCS_dequeuestateq (lcu_StateQ *q);

LCUI_FUNC lua_State *lcuCS_removestateq (lcu_StateQ *q, lua_State *L);


LCUI_FUNC int lcuCS_checksyncargs (lua_State *L);

typedef struct lcu_ChannelSync lcu_ChannelSync;

#define LCU_CHSYNCIN	0x01
#define LCU_CHSYNCOUT	0x02
#define LCU_CHSYNCANY	(LCU_CHSYNCIN|LCU_CHSYNCOUT)

typedef lua_State *(*lcu_GetAsyncState) (lua_State *L, void *userdata);

LCUI_FUNC int lcuCS_matchchsync (lcu_ChannelSync *sync,
                                 int endpoint,
                                 lua_State *L,
                                 lcu_GetAsyncState getstate,
                                 void *userdata);


typedef struct lcu_ChannelMap lcu_ChannelMap;

LCUI_FUNC lcu_ChannelMap *lcuCS_tochannelmap (lua_State *L);

LCUI_FUNC lcu_ChannelSync *lcuCS_getchsync (lcu_ChannelMap *map,
                                            const char *name);

LCUI_FUNC void lcuCS_freechsync (lcu_ChannelMap *map, const char *name);



/*
 * lchtask.h
 */

#define LCU_CHANNELTASKCLS	LCU_PREFIX"ChannelTask"

#define LCU_CHANNELSYNCREGKEY	LCU_PREFIX"uv_async_t channelWake"

LCUI_FUNC lua_State *lcuCT_getsuspendedtask (lua_State *L);

LCUI_FUNC int lcuCT_suspendedchtask (lua_State *L);



/*
 * lthpool.h
 */

typedef struct lcu_ThreadPool lcu_ThreadPool;

typedef struct lcu_ThreadCount {
	int expected;
	int actual;
	int running;
	int pending;
	int suspended;
	int numoftasks;
} lcu_ThreadCount;


LCUI_FUNC int lcuTP_createtpool (lcu_ThreadPool **ref,
                                 lua_Alloc allocf,
                                 void *allocud);

LCUI_FUNC void lcuTP_destroytpool (lcu_ThreadPool *pool);

LCUI_FUNC void lcuTP_closetpool (lcu_ThreadPool *pool);

LCUI_FUNC int lcuTP_resizetpool (lcu_ThreadPool *pool, int size, int create);

LCUI_FUNC int lcuTP_addtasktpool (lcu_ThreadPool *pool, lua_State *L);

LCUI_FUNC int lcuTP_counttpool (lcu_ThreadPool *pool,
                                lcu_ThreadCount *count,
                                const char *what);

LCUI_FUNC void lcuTP_resumetask (lua_State *L);



/*
 * lchsync.c
 */

#define REGKEY_NEXTSTATE	LCU_PREFIX"lua_State nextQueuedTask"

struct lcu_StateQ {
	lua_State *head;
	lua_State *tail;
};

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

static void appendstateq (lcu_StateQ *q, lcu_StateQ *q2) {
	if (q->head) {
		lcu_assert(lua_checkstack(q->tail, 1));
		setnextstateq(q->tail, q2->head);
	} else {
		q->head = q2->head;
	}
	q->tail = q2->tail;
}

LCUI_FUNC void lcuCS_initstateq (lcu_StateQ *q) {
	q->head = NULL;
	q->tail = NULL;
}

LCUI_FUNC int lcuCS_emptystateq (lcu_StateQ *q) {
	return q->head == NULL;
}

LCUI_FUNC void lcuCS_enqueuestateq (lcu_StateQ *q, lua_State *L) {
	lcu_StateQ single = { L, L };
	appendstateq(q, &single);
}

LCUI_FUNC lua_State *lcuCS_dequeuestateq (lcu_StateQ *q) {
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

LCUI_FUNC lua_State *lcuCS_removestateq (lcu_StateQ *q, lua_State *L) {
	lua_State *prev = q->head;
	if (prev != NULL) {
		if (L == prev) return lcuCS_dequeuestateq(q);
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


struct lcu_ChannelSync {
	uv_mutex_t mutex;
	int refcount;
	int expected;
	lcu_StateQ queue;
};

LCUI_FUNC int lcuCS_checksyncargs (lua_State *L) {
	static const char *const options[] = { "in", "out", "any" };
	static const int endpoints[] = { LCU_CHSYNCIN, LCU_CHSYNCOUT, LCU_CHSYNCANY };
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

LCUI_FUNC int lcuCS_matchchsync (lcu_ChannelSync *sync,
                                 int endpoint,
                                 lua_State *L,
                                 lcu_GetAsyncState getstate,
                                 void *userdata) {
	uv_mutex_lock(&sync->mutex);
	if (sync->queue.head == NULL) {
		sync->expected = endpoint == LCU_CHSYNCANY ? LCU_CHSYNCANY
		                                           : endpoint^LCU_CHSYNCANY;
	} else if (sync->expected&endpoint) {
		lua_State *match = lcuCS_dequeuestateq(&sync->queue);
		uv_mutex_unlock(&sync->mutex);
		swapvalues(L, match);  /* 'match' may be a task or a channel */
		match = lcuCT_getsuspendedtask(match);  /* if a channel, gets its task */
		if (match) lcuTP_resumetask(match);
		return 1;
	}
	if (getstate != NULL) {
		L = getstate(L, userdata);
		if (L == NULL) goto match_end;
	}
	lcuCS_enqueuestateq(&sync->queue, L);

	match_end:
	uv_mutex_unlock(&sync->mutex);
	return L == NULL;
}

struct lcu_ChannelMap {
	uv_mutex_t mutex;
	lua_State *L;
};

static int channelmap_gc (lua_State *L) {
	lcu_ChannelMap *map = (lcu_ChannelMap *)lua_touserdata(L, 1);
	lcu_StateQ queue;
	lcuCS_initstateq(&queue);

	uv_mutex_lock(&map->mutex);
	L = map->L;
	lcu_assert(L);
	lcu_assert(lua_gettop(L) == 0);
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L, 1)) {
		const char *name = lua_tostring(L, -2);
		if (name) {
			lcu_ChannelSync *sync = (lcu_ChannelSync *)lua_touserdata(L, -1);
			uv_mutex_lock(&sync->mutex);
			if (!lcuCS_emptystateq(&sync->queue)) {
				appendstateq(&queue, &sync->queue);
				lcuCS_initstateq(&sync->queue);
			}
			uv_mutex_unlock(&sync->mutex);
		}
		lua_pop(L, 1);
	}
	lua_settop(L, 0);  /* remove global table */
	uv_mutex_unlock(&map->mutex);

	while ((L = lcuCS_dequeuestateq(&queue))) lua_close(L);

	uv_mutex_destroy(&map->mutex);
	lua_close(map->L);
	map->L = NULL;

	return 0;
}

LCUI_FUNC lcu_ChannelMap *lcuCS_tochannelmap (lua_State *L) {
	lcu_ChannelMap *map;
	int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
	if (type == LUA_TNIL) {
		map = (lcu_ChannelMap *)lua_newuserdatauv(L, sizeof(lcu_ChannelMap), 0);
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		map->L = NULL;
		lcuL_setfinalizer(L, channelmap_gc);
		map->L = lua_newstate(allocf, allocud);
		if (map->L == NULL) luaL_error(L, "not enough memory");
		uv_mutex_init(&map->mutex);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSREGKEY);
	} else {
		map = (lcu_ChannelMap *)lua_touserdata(L, -1);
		lcu_assert(map);
	}
	lua_pop(L, 1);
	return map;
}



/* TODO: remove this */ LCUI_FUNC void lcuM_addchanelg (lua_State *L) { lcuCS_tochannelmap(L); }



LCUI_FUNC lcu_ChannelSync *lcuCS_getchsync (lcu_ChannelMap *map,
                                            const char *name) {
	lua_State *L = map->L;
	lcu_ChannelSync *sync = NULL;
	uv_mutex_lock(&map->mutex);
	if (lua_getglobal(L, name) == LUA_TLIGHTUSERDATA) {
		sync = (lcu_ChannelSync *)lua_touserdata(L, -1);
		uv_mutex_lock(&sync->mutex);
		sync->refcount++;
		uv_mutex_unlock(&sync->mutex);
	} else {
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		sync = (lcu_ChannelSync *)allocf(allocud, NULL, 0, sizeof(lcu_ChannelSync));
		if (sync) {
			uv_mutex_init(&sync->mutex);
			sync->refcount = 1;
			sync->expected = 0;
			lcuCS_initstateq(&sync->queue);;
			lua_pushlightuserdata(L, sync);
			lua_setglobal(L, name);
		}
	}
	lua_pop(L, 1);
	uv_mutex_unlock(&map->mutex);
	return sync;
}

LCUI_FUNC void lcuCS_freechsync (lcu_ChannelMap *map, const char *name) {
	lua_State *L = map->L;
	lcu_ChannelSync *sync = NULL;
	uv_mutex_lock(&map->mutex);
	if (lua_getglobal(L, name) == LUA_TLIGHTUSERDATA) {
		sync = (lcu_ChannelSync *)lua_touserdata(L, -1);
		if (--sync->refcount == 0 && lcuCS_emptystateq(&sync->queue)) {
			void *allocud;
			lua_Alloc allocf = lua_getallocf(L, &allocud);
			uv_mutex_destroy(&sync->mutex);
			allocf(allocud, sync, sizeof(lcu_ChannelSync), 0);
			lua_pushnil(L);
			lua_setglobal(L, name);
		}
	}
	lua_pop(L, 1);
	uv_mutex_unlock(&map->mutex);
}



/*
 * lthpool.c
 */

typedef enum { TPOOL_OPEN, TPOOL_CLOSING, TPOOL_CLOSED } TPoolStatus;

struct lcu_ThreadPool {
	lua_Alloc allocf;
	void *allocud;
	uv_mutex_t mutex;
	uv_cond_t onwork;
	uv_cond_t onterm;
	TPoolStatus status;
	int size;  /* expected number of system threads */
	int threads;  /* current number of system threads */
	int idle;  /* number of system threads waiting on 'onwork' */
	int tasks;  /* total number of tasks (coroutines) in the thread pool */
	int running;  /* number of system threads running tasks */
	int pending;  /* number of tasks in 'queue' */
	lcu_StateQ queue;
};


static int hasextraidle_mx (lcu_ThreadPool *pool) {
	return pool->threads > pool->size && pool->idle > 0;
}

static void checkhalted_mx (lcu_ThreadPool *pool) {
	if (pool->tasks == 0 && pool->size > 0) {
		pool->size = 0;
		uv_cond_signal(&pool->onwork);
	}
}

static void threadmain (void *arg);

static int addthread_mx (lcu_ThreadPool *pool, lua_State *L) {
	/* starting or waking threads won't get this new task */
	if (pool->threads-pool->running-pool->idle <= pool->pending) {
		if (pool->idle > 0) {
			pool->idle--;
			uv_cond_signal(&pool->onwork);
		} else if (pool->threads < pool->size) {
			uv_thread_t tid;
			int err = uv_thread_create(&tid, threadmain, pool);
			if (!err) pool->threads++;
			else lcuL_warnerr(L, "system.threads: ", err);
		}
	}
	if (pool->status == TPOOL_CLOSED) return 0;
	lcuCS_enqueuestateq(&pool->queue, L);
	pool->pending++;
	return 1;
}

static void threadmain (void *arg) {
	lcu_ThreadPool *pool = (lcu_ThreadPool *)arg;
	lua_State *L = NULL;

	uv_mutex_lock(&pool->mutex);
	while (1) {
		int narg, status;
		while (1) {
			if (pool->threads > pool->size) {
				goto thread_end;
			} else if (pool->pending) {
				pool->pending--;
				L = lcuCS_dequeuestateq(&pool->queue);
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
		status = lua_resume(L, NULL, narg, &narg);
		if (status == LUA_YIELD) {
			const char *channelname = lua_tostring(L, 1);
			if (channelname) {
				lcu_ChannelMap *map = lcuCS_tochannelmap(L);
				lcu_ChannelSync *sync = lcuCS_getchsync(map, channelname);
				int endpoint = lcuCS_checksyncargs(L);
				if (endpoint != -1 && !lcuCS_matchchsync(sync, endpoint, L, NULL, NULL))
					L = NULL;
				lcuCS_freechsync(map, channelname);
			} else if (lcuCT_suspendedchtask(L)) {
				L = NULL;
			} else {
				lua_settop(L, 0);  /* discard returned values */
			}
		} else {
			if (status != LUA_OK) {
				lcuL_warnmsg(L, "threads: ", lua_tostring(L, -1));
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
			lcuCS_enqueuestateq(&pool->queue, L);
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



LCUI_FUNC void lcuTP_resumetask (lua_State *L) {
	lcu_ThreadPool *pool;
	int added;
	lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
	pool = *((lcu_ThreadPool **)lua_touserdata(L, -1));
	lua_pop(L, 1);
	uv_mutex_lock(&pool->mutex);
	added = addthread_mx(pool, L);
	uv_mutex_unlock(&pool->mutex);
	if (!added) lua_close(L);
}



LCUI_FUNC int lcuTP_createtpool (lcu_ThreadPool **ref,
                                 lua_Alloc allocf,
                                 void *allocud) {
	int err = UV_ENOMEM;
	lcu_ThreadPool *pool = (lcu_ThreadPool *)allocf(allocud, NULL, 0, sizeof(lcu_ThreadPool));
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
	lcuCS_initstateq(&pool->queue);
	*ref = pool;
	return 0;

	ontermcond_err:
	uv_cond_destroy(&pool->onwork);
	onworkcond_err:
	uv_mutex_destroy(&pool->mutex);
	mutex_err:
	allocf(allocud, pool, sizeof(lcu_ThreadPool), 0);
	alloc_err:
	return err;
}

LCUI_FUNC void lcuTP_destroytpool (lcu_ThreadPool *pool) {
	uv_mutex_destroy(&pool->mutex);
	pool->allocf(pool->allocud, pool, sizeof(lcu_ThreadPool), 0);
}

LCUI_FUNC void lcuTP_closetpool (lcu_ThreadPool *pool) {
	int tasks, pending;
	lcu_StateQ queue;

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
	lcuCS_initstateq(&pool->queue);
	pool->status = TPOOL_CLOSED;
	uv_mutex_unlock(&pool->mutex);

	uv_cond_destroy(&pool->onterm);
	uv_cond_destroy(&pool->onwork);

	if (tasks == 0) {
		lcuTP_destroytpool(pool);
	} else if (pending > 0) {
		lua_State *L;
		while ((L = lcuCS_dequeuestateq(&queue))) lua_close(L);
	}
}

LCUI_FUNC int lcuTP_resizetpool (lcu_ThreadPool *pool, int size, int create) {
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
	lcu_ThreadPool *pool = *((lcu_ThreadPool **)lua_touserdata(L, 1));
	int status, tasks;
	uv_mutex_lock(&pool->mutex);
	status = pool->status;
	tasks = --pool->tasks;
	uv_mutex_unlock(&pool->mutex);
	if (status == TPOOL_CLOSED && tasks == 0) lcuTP_destroytpool(pool);
	return 0;
}

LCUI_FUNC int lcuTP_addtasktpool (lcu_ThreadPool *pool, lua_State *L) {
	lcu_ThreadPool **poolref;
	int added;
	int hasspace = lua_checkstack(L, 1);
	lcu_assert(hasspace);
	poolref = (lcu_ThreadPool **)lua_newuserdatauv(L, sizeof(lcu_ThreadPool *), 0);
	*poolref = pool;
	lcuL_setfinalizer(L, collectthreadpool);
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);

	uv_mutex_lock(&pool->mutex);
	added = addthread_mx(pool, L);
	lcu_assert(added);
	pool->tasks++;
	uv_mutex_unlock(&pool->mutex);

	return 0;
}

LCUI_FUNC int lcuTP_counttpool (lcu_ThreadPool *pool,
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



/*
 * lthreadm.c
 */

#define CLASS_TPOOLGC	LCU_PREFIX"lcu_ThreadPool *"
#define CLASS_THREADS	LCU_PREFIX"threads"


static lcu_ThreadPool *tothreads (lua_State *L, int idx) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, idx, CLASS_THREADS);
	luaL_argcheck(L, *ref, idx, "closed threads");
	return *ref;
}

/* getmetatable(tpoolgc).__gc(pool) */
static int tpoolgc_gc (lua_State *L) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, 1, CLASS_TPOOLGC);
	lcuTP_closetpool(*ref);
	return 0;
}

/* succ [, errmsg] = threads:close() */
static int threads_close (lua_State *L) {
	lcu_ThreadPool **ref = (lcu_ThreadPool **)luaL_checkudata(L, 1, CLASS_THREADS);
	lua_pushboolean(L, *ref != NULL);
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
	status = lcuL_movefrom(NL, L, top > narg ? top-narg : 0, "argument");
	if (status != LUA_OK) {
		const char *msg = lua_tostring(NL, -1);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, msg);
		lua_close(NL);
		return 2;  /* return nil plus error message */
	}
	status = lcuTP_addtasktpool(pool, NL);
	if (status) {
		lua_close(NL);
		return lcuL_pusherrres(L, status);
	}
	lua_pushboolean(L, 1);
	return 1;
}

/* succ [, errmsg] = threads:dostring(chunk [, chunkname [, mode, ...]]) */
static int threads_dostring (lua_State *L) {
	lcu_ThreadPool *pool = tothreads(L, 1);
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
		luaL_setmetatable(L, CLASS_TPOOLGC);
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
		luaL_setmetatable(L, CLASS_THREADS);
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
	luaL_newlib(L, modulef);
	luaL_newmetatable(L, CLASS_TPOOLGC)  /* metatable for tpool sentinel */;
	luaL_setfuncs(L, poolrefmt, 0);  /* add metamethods to metatable */
	lua_pop(L, 1);  /* pop metatable */
	luaL_newmetatable(L, CLASS_THREADS);  /* metatable for thread pools */
	luaL_setfuncs(L, threadsmt, 0);  /* add metamethods to metatable */
	lua_pushvalue(L, -2);  /* push library */
	lua_setfield(L, -2, "__index");  /* metatable.__index = library */
	lua_pop(L, 1);  /* pop metatable */
	return 1;
}



/*
 * lchtask.c
 */

typedef struct ChannelTask {
	uv_mutex_t mutex;
	int wakes;
	lua_State *L;
} ChannelTask;

LCUI_FUNC int lcuCT_suspendedchtask (lua_State *L) {
	ChannelTask *channeltask = (ChannelTask *)luaL_testudata(L, 1, LCU_CHANNELTASKCLS);
	if (channeltask == NULL) return 0;
	uv_mutex_lock(&channeltask->mutex);
	if (channeltask->wakes == 0) {
		channeltask->L = L;
		L = NULL;
	}
	uv_mutex_unlock(&channeltask->mutex);
	return L == NULL;
}

LCUI_FUNC lua_State *lcuCT_getsuspendedtask (lua_State *L) {
	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TLIGHTUSERDATA) {
		uv_async_t *async;
		ChannelTask *channeltask = (ChannelTask *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
		async = (uv_async_t *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		uv_mutex_lock(&channeltask->mutex);
		L = channeltask->L;
		channeltask->L = NULL;
		channeltask->wakes++;
		uv_mutex_unlock(&channeltask->mutex);
		uv_async_send(async);
	}
	else lua_pop(L, 1);
	return L;
}



/*
 * lchannelm.c
 */

#define CLASS_CHANNEL	LCU_PREFIX"channel"

typedef struct LuaChannel {
	lcu_ChannelSync *sync;
	lua_State *L;
	uv_async_t *handle;
} LuaChannel;

#define tolchannel(L,I)	((LuaChannel *)luaL_checkudata(L,I,CLASS_CHANNEL))

static LuaChannel *chklchannel(lua_State *L, int arg) {
	LuaChannel *channel = tolchannel(L, arg);
	int ltype = lua_getiuservalue(L, arg, 1);
	luaL_argcheck(L, ltype == LUA_TSTRING, arg, "closed channel");
	lua_pop(L, 1);
	return channel;
}

static int channelclose (lua_State *L, LuaChannel *channel) {
	lcu_ChannelMap *map = lcuCS_tochannelmap(L);
	if (lua_getuservalue(L, 1) == LUA_TSTRING) {
		const char *name = lua_tostring(L, -1);
		if (channel->handle) {
			/* whole lua_State is closing, but still waiting on channel */
			lua_State *cL;
			lcu_ChannelSync *sync = channel->sync;
			uv_mutex_lock(&sync->mutex);
			cL = lcuCS_removestateq(&sync->queue, channel->L);
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
		lcuCS_freechsync(map, name);
		/* DEBUG: channel->sync = NULL; */
		/* DEBUG: channel->L = NULL; */
		/* DEBUG: channel->handle = NULL; */
		lua_pushnil(L);
		lua_setuservalue(L, 1);
		return 1;
	}
	return 0;
}

static int channelsync (lcu_ChannelSync *sync,
                        lua_State *L,
                        lcu_GetAsyncState getstate,
                        void *userdata) {
	int endpoint = lcuCS_checksyncargs(L);
	if (endpoint == -1) lua_error(L);
	return lcuCS_matchchsync(sync, endpoint, L, getstate, userdata);
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
	lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
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
	lcu_ChannelSync *sync = channel->sync;
	lua_State *cL = channel->L;
	uv_mutex_lock(&sync->mutex);
	cL = lcuCS_removestateq(&sync->queue, cL);
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
		lcuU_resumethrop(handle, 0);
	} else {
		LuaChannel *channel = (LuaChannel *)lua_touserdata(thread, 1);
		restorechannel(channel);
	}
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
	lua_setfield(cL, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
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
	if (channelsync(channel->sync, L, armsynced, &args)) return lua_gettop(L);
	return -1;
}

static int system_awaitch (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetthropk(L, UV_ASYNC, sched, k_setupsynced,
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
	channelsync(channel->sync, L, cancelsuspension, NULL);
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

/* channel [, errmsg] = channel.create(name) */
static int channel_create (lua_State *L) {
	lcu_ChannelMap *map = lcuCS_tochannelmap(L);
	const char *name = luaL_checkstring(L, 1);
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	LuaChannel *channel = (LuaChannel *)lua_newuserdatauv(L, sizeof(LuaChannel), 1);
	ChannelTask *channeltask;

	/* save channel name */
	lua_pushvalue(L, 1);
	lua_setiuservalue(L, -2, 1);

	channel->handle = NULL;
	channel->L = lua_newstate(allocf, allocud);
	if (channel->L == NULL) luaL_error(L, "not enough memory");
	channel->sync = lcuCS_getchsync(map, name);
	if (channel->sync == NULL) {
		lua_close(channel->L);
		luaL_error(L, "not enough memory");
	}
	luaL_setmetatable(L, CLASS_CHANNEL);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TNIL) {
		int err;
		channeltask = (ChannelTask *)lua_newuserdatauv(L, sizeof(ChannelTask), 0);
		channeltask->wakes = 0;
		channeltask->L = NULL;
		err = uv_mutex_init(&channeltask->mutex);
		if (err) return lcuL_pusherrres(L, err);
		luaL_setmetatable(L, LCU_CHANNELTASKCLS);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
	} else {
		channeltask = (ChannelTask *)lua_touserdata(L, -1);
	}
	lua_pop(L, 1);

	lua_pushlightuserdata(channel->L, channeltask);
	lua_setfield(channel->L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);

	return 1;
}

/* names [, errmsg] = channel.getnames([names]) */
static int channel_getnames (lua_State *L) {
	lcu_ChannelMap *map = lcuCS_tochannelmap(L);
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



LCUMOD_API int luaopen_coutil_channel (lua_State *L) {
	static const luaL_Reg channeltaskf[] = {
		{"__gc", channeltask_gc},
		{NULL, NULL}
	};
	static const luaL_Reg channelf[] = {
		{"__gc", channel_gc},
		{"__close", channel_gc},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"getnames", channel_getnames},
		{"create", channel_create},
		{"close", channel_close},
		{"sync", channel_sync},
		{"getname", channel_getname},
		{NULL, NULL}
	};
	luaL_newlib(L, modf);
	luaL_newmetatable(L, LCU_CHANNELTASKCLS);
	luaL_setfuncs(L, channeltaskf, 0);  /* add metamethods to metatable */
	lua_pop(L, 1);  /* pop metatable */
	luaL_newmetatable(L, CLASS_CHANNEL);
	luaL_setfuncs(L, channelf, 0);  /* add metamethods to metatable */
	lua_pushvalue(L, -2);  /* push library */
	lua_setfield(L, -2, "__index");  /* metatable.__index = library */
	lua_pop(L, 1);  /* pop metatable */
	return 1;
}

LCUI_FUNC void lcuM_addchanelf (lua_State *L) {
	static const luaL_Reg upvf[] = {
		{"awaitch", system_awaitch},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, upvf, LCU_MODUPVS);
}
