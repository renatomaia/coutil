#include "lthpool.h"

#include "lmodaux.h"
#include "lchaux.h"

#include <uv.h>


#define STATUS_MASK     0x03
#define STATUS_OPEN     0x00
#define STATUS_CLOSING  0x01
#define STATUS_CLOSED   0x02
#define JOIN_PENDING    0x04  /* flat when 'last_terminated' is set */

#define getstatus(P)  ((P)->flags&STATUS_MASK)
#define setstatus(P,V)  ((P)->flags = ((P)->flags & (~STATUS_MASK)) | (V))

struct lcu_ThreadPool {
	lua_Alloc allocf;
	void *allocud;
	uv_mutex_t mutex;
	uv_cond_t onwork;
	uv_cond_t onterm;
	int flags;  /* STATUS_MASK|JOIN_PENDING */
	int size;  /* expected number of system threads */
	int threads;  /* current number of system threads */
	int idle;  /* number of system threads waiting on 'onwork' */
	int tasks;  /* total number of tasks (coroutines) in the thread pool */
	int running;  /* number of system threads running tasks */
	int pending;  /* number of tasks in 'queue' */
	lcu_StateQ queue;
	uv_thread_t last_terminated;  /* terminated worker thread pending join */
};


static int hasextraidle_mx (lcu_ThreadPool *pool) {
	return pool->threads > pool->size && pool->idle > 0;
}

static void checkhalted_mx (lcu_ThreadPool *pool) {
	if (pool->tasks == 0 && pool->size > 0) {
		pool->size = 0;
		uv_cond_broadcast(&pool->onwork);
	}
}

static void threadmain (void *arg);

static int addthread_mx (lcu_ThreadPool *pool, lua_State *L) {
	/* starting or waking threads won't get this new task */
	if (pool->threads-pool->running-pool->idle <= pool->pending) {
		if (pool->idle > 0) {
			uv_cond_signal(&pool->onwork);
		} else if (pool->threads < pool->size) {
			uv_thread_t tid;
			int err = uv_thread_create(&tid, threadmain, pool);
			if (!err) pool->threads++;
			else lcuL_warnerr(L, "system.threads", err);
		}
	}
	if (getstatus(pool) == STATUS_CLOSED) return 0;
	lcuCS_enqueuestateq(&pool->queue, L);
	pool->pending++;
	return 1;
}

static void threadmain (void *arg) {
	lcu_ThreadPool *pool = (lcu_ThreadPool *)arg;

	uv_mutex_lock(&pool->mutex);
	while (1) {
		lua_State *L = NULL;
		int narg, status, enqueue;
		while (1) {
			if (pool->threads > pool->size) {
				goto thread_end;
			} else if (pool->pending) {
				pool->pending--;
				L = lcuCS_dequeuestateq(&pool->queue);
				break;
			} else if (getstatus(pool) == STATUS_CLOSING && pool->tasks == 0) {  /* if halted? */
				pool->size = 0;
			} else {
				pool->idle++;
				uv_cond_wait(&pool->onwork, &pool->mutex);
				pool->idle--;
			}
		}
		pool->running++;
		uv_mutex_unlock(&pool->mutex);
		if (lua_status(L) == LUA_OK) narg = lua_gettop(L)-1;
		else {
			narg = lua_tointeger(L, -1);
			lua_pop(L, 1);  /* discard 'narg' */
		}
		status = lua_resume(L, NULL, narg, &narg);
		if (status == LUA_YIELD) {
			int base = lua_gettop(L)-narg;
			const char *channelname = lua_tostring(L, base+1);
			if (channelname) {
				lcu_ChannelMap *map = lcuCS_tochannelmap(L);
				lcu_ChannelSync *sync = lcuCS_getchsync(map, channelname);
				int endpoint = lcuCS_checksyncargs(L, base+2);
				if (endpoint == -1) {
					enqueue = 1;
					lcu_assert(lua_gettop(L) > base+2);
					lua_replace(L, base+2);  /* place errmsg as 2nd return */
					lua_pushboolean(L, 0);
					lua_replace(L, base+1);  /* place false as 1st return */
					lua_settop(L, base+2);  /* discard other values */
					lua_pushinteger(L, 2);  /* push 'narg' */
				} else {
					enqueue = lcuCS_matchchsync(sync, endpoint, L, base, narg, NULL, NULL);
				}
				lcuCS_freechsync(map, channelname);
			} else {
				enqueue = !lcuCS_suspendedchtask(L, base+1);
				lua_settop(L, base);  /* discard returned values */
				lua_pushinteger(L, 0);  /* push 'narg' */
			}
		} else {
			enqueue = 0;
			if (status != LUA_OK) lcuL_warnmsg(L, "threads", lua_tostring(L, -1));
			/* avoid 'pool->tasks--' */
			lua_settop(L, 0);
			lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY);
			lua_pushnil(L);
			lua_setmetatable(L, -2);
			lua_close(lcuL_tomain(L));
		}

		uv_mutex_lock(&pool->mutex);
		pool->running--;
		if (enqueue) {
			pool->pending++;
			lcuCS_enqueuestateq(&pool->queue, L);
		} else if (status != LUA_YIELD) {
			pool->tasks--;
		}
	}
	thread_end:
	pool->threads--;
	if (hasextraidle_mx(pool)) {
		uv_cond_signal(&pool->onwork);
	} else if (getstatus(pool) == STATUS_CLOSING) {
		if (pool->threads == 0) uv_cond_signal(&pool->onterm);
		else checkhalted_mx(pool);
	}

	if (lcuL_maskflag(pool, JOIN_PENDING)) uv_thread_join(&pool->last_terminated);
	else lcuL_setflag(pool, JOIN_PENDING);
	pool->last_terminated = uv_thread_self();

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
	if (!added) lua_close(lcuL_tomain(L));
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
	pool->flags = STATUS_OPEN;
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
	lcu_assert(getstatus(pool) == STATUS_CLOSED);
	lcu_assert(pool->threads == 0);
	lcu_assert(pool->idle == 0);
	lcu_assert(pool->tasks == 0);
	lcu_assert(pool->running == 0);
	lcu_assert(pool->pending == 0);
	lcu_assert(pool->queue.head == NULL);
	lcu_assert(pool->queue.tail == NULL);
	uv_mutex_destroy(&pool->mutex);
	if (lcuL_maskflag(pool, JOIN_PENDING)) uv_thread_join(&pool->last_terminated);
	pool->allocf(pool->allocud, pool, sizeof(lcu_ThreadPool), 0);
}

LCUI_FUNC void lcuTP_closetpool (lcu_ThreadPool *pool) {
	int tasks, pending;
	lcu_StateQ queue;

	uv_mutex_lock(&pool->mutex);
	setstatus(pool, STATUS_CLOSING);
	while (pool->threads > 0) {
		checkhalted_mx(pool);
		uv_cond_wait(&pool->onterm, &pool->mutex);
	}
	tasks = pool->tasks;
	pending = pool->pending;
	queue = pool->queue;
	pool->pending = 0;
	lcuCS_initstateq(&pool->queue);
	setstatus(pool, STATUS_CLOSED);
	uv_mutex_unlock(&pool->mutex);

	uv_cond_destroy(&pool->onterm);
	uv_cond_destroy(&pool->onwork);

	if (tasks == 0) {
		lcuTP_destroytpool(pool);
	} else if (pending > 0) {
		lua_State *L;
		while ((L = lcuCS_dequeuestateq(&queue))) lua_close(lcuL_tomain(L));
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
		uv_cond_signal(&pool->onwork);
	}
	uv_mutex_unlock(&pool->mutex);

	return err;
}

static int collectthreadpool (lua_State *L) {
	lcu_ThreadPool *pool = *((lcu_ThreadPool **)lua_touserdata(L, 1));
	int status, tasks;
	uv_mutex_lock(&pool->mutex);
	status = getstatus(pool);
	tasks = --pool->tasks;
	uv_mutex_unlock(&pool->mutex);
	if (status == STATUS_CLOSED && tasks == 0) lcuTP_destroytpool(pool);
	return 0;
}

LCUI_FUNC int lcuTP_addtpooltask (lcu_ThreadPool *pool, lua_State *L) {
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
	for (; *what; what++) switch (*what) {
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
