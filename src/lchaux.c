#include "lchaux.h"

#include "lmodaux.h"
#include "lchdefs.h"
#include "lthpool.h"

#include <string.h>
#include <lauxlib.h>
#include <uv.h>


#define REGKEY_NEXTSTATE	LCU_PREFIX"lua_State nextQueuedTask"


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


LCUI_FUNC int lcuCS_checksyncargs (lua_State *L, int idx) {
	static const char *const options[] = { "in", "out", "any", NULL };
	static const int endpoints[] = { LCU_CHSYNCIN, LCU_CHSYNCOUT, LCU_CHSYNCANY };
	const char *name = luaL_optstring(L, idx, "any");
	int i;
	for (i = 0; options[i]; i++) {
		if (strcmp(options[i], name) == 0) {
			int narg = lua_gettop(L)-idx;
			if (narg <= 0 || lcuL_canmove(L, narg, "argument")) return endpoints[i];
			return -1;
		}
	}
	lua_pushfstring(L, "bad argument #2 (invalid option '%s')", name);
	return -1;
}

static void pusherrfrom (lua_State *L, int base, lua_State *failed, int bfailed) {
	lua_replace(failed, bfailed+1);
	lua_settop(failed, bfailed+1);
	lua_pushboolean(failed, 0);
	lua_insert(failed, bfailed+1);
	lua_pushinteger(failed, 2);  /* push narg */

	lua_settop(L, base);
	lua_pushboolean(L, 0);
	lcuL_pushfrom(NULL, L, failed, -1, "error");
	lua_pushinteger(L, 2);  /* push narg */
}

static void swapvalues (lua_State *src, int bsrc, int nsrc, lua_State *dst) {
	int bdst = lua_tointeger(dst, -2);
	int ndst = lua_tointeger(dst, -1);
	int err;

	lua_pop(dst, 2);  /* discard 'ndst' and 'ndst' from top */

	if (nsrc < ndst) {
		{ lua_State *L = dst; dst = src; src = L; }
		{ int n = ndst; ndst = nsrc; nsrc = n; }
		{ int b = bdst; bdst = bsrc; bsrc = b; }
	}

	if (ndst) {
		int tsrc = lua_gettop(src)-nsrc;
		int tdst = lua_gettop(dst)-ndst;
		int i;

		lcu_assert(bsrc < tsrc);
		lcu_assert(bdst < tdst);

		for (i = 1; i <= ndst; i++) {
			err = lcuL_pushfrom(NULL, src, dst, tdst+i, "argument");
			if (err != LUA_OK) {
				pusherrfrom(dst, bdst, src, bsrc);
				return;
			}
			err = lcuL_pushfrom(NULL, dst, src, tsrc+i, "argument");
			if (err != LUA_OK) {
				pusherrfrom(src, bsrc, dst, bdst);
				return;
			}
			lua_replace(src, bsrc+i+1);
			lua_replace(dst, bdst+i+1);
		}
	}

	lua_settop(dst, bdst+ndst+1);

	err = lcuL_movefrom(NULL, dst, src, nsrc-ndst, "argument");
	if (err != LUA_OK) pusherrfrom(src, bsrc, dst, bdst);
	else {
		lua_settop(src, bsrc+ndst+1);

		lua_pushboolean(src, 1);
		lua_replace(src, bsrc+1);
		lcu_assert(lua_gettop(src) == bsrc+ndst+1);
		lua_pushinteger(src, ndst+1);  /* push narg */

		lua_pushboolean(dst, 1);
		lua_replace(dst, bdst+1);
		lcu_assert(lua_gettop(dst) == bdst+nsrc+1);
		lua_pushinteger(dst, nsrc+1);  /* push narg */
	}
}

static lua_State *getsuspendedtask (lua_State *L) {
	/* only the state of a channel has a light userdata in 'LCU_CHANNELTASKREGKEY' */
	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TLIGHTUSERDATA) {
		uv_async_t *async;
		lcu_ChannelTask *channeltask = (lcu_ChannelTask *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
		async = (uv_async_t *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		uv_async_send(async);
		if (channeltask == NULL) L = NULL;
		else {
			uv_mutex_lock(&channeltask->mutex);
			L = channeltask->L;
			channeltask->L = NULL;
			channeltask->wakes++;
			uv_mutex_unlock(&channeltask->mutex);
		}
	}
	else lua_pop(L, 1);  /* 'L' a task (value is either full userdata or false) */
	return L;
}

LCUI_FUNC int lcuCS_matchchsync (lcu_ChannelSync *sync,
                                 int endpoint,
                                 lua_State *L,
                                 int base,
                                 int narg,
                                 lcu_GetAsyncState getstate,
                                 void *userdata) {
	narg = narg > 2 ? narg-2 : 0;  /* exclude 'channel' and 'endpoint' args */
	uv_mutex_lock(&sync->mutex);
	if (sync->queue.head == NULL) {
		sync->expected = endpoint == LCU_CHSYNCANY ? LCU_CHSYNCANY
		                                           : endpoint^LCU_CHSYNCANY;
	} else if (sync->expected&endpoint) {
		lua_State *match = lcuCS_dequeuestateq(&sync->queue);
		uv_mutex_unlock(&sync->mutex);
		swapvalues(L, base, narg, match);  /* 'match' may be a task or a channel */
		match = getsuspendedtask(match);  /* if a channel, gets its task */
		if (match) lcuTP_resumetask(match);
		return 1;
	}
	if (getstate != NULL) {
		L = getstate(L, userdata);
		if (L == NULL) goto match_end;
	} else {
		/* push 'base' and 'narg' to be used in 'swapvalues' */
		lua_pushinteger(L, base);
		lua_pushinteger(L, narg);
	}
	lcuCS_enqueuestateq(&sync->queue, L);

	match_end:
	uv_mutex_unlock(&sync->mutex);
	return L == NULL;
}

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
		void *allocud;
		lua_Alloc allocf = lua_getallocf(L, &allocud);
		map = (lcu_ChannelMap *)lua_newuserdatauv(L, sizeof(lcu_ChannelMap), 0);
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


LCUI_FUNC int lcuCS_suspendedchtask (lua_State *L, int idx) {
	lcu_ChannelTask *channeltask = (lcu_ChannelTask *)luaL_testudata(L, idx, LCU_CHANNELTASKCLS);
	if (channeltask == NULL) return 0;
	uv_mutex_lock(&channeltask->mutex);
	if (channeltask->wakes == 0) {
		channeltask->L = L;
		L = NULL;
	}
	uv_mutex_unlock(&channeltask->mutex);
	return L == NULL;
}
