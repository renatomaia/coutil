#include "lthpool.h"

#include "lmodaux.h"
#include "lchaux.h"

#include <uv.h>


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
