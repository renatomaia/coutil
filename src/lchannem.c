#include "lmodaux.h"
#include "loperaux.h"
#include "lchdefs.h"


typedef struct LuaChannel {
	lcu_ChannelSync *sync;
	lua_State *L;
	uv_async_t *handle;
} LuaChannel;

#define tolchannel(L,I)	((LuaChannel *)luaL_checkudata(L,I,LCU_CHANNELCLS))

static LuaChannel *chklchannel(lua_State *L, int arg) {
	LuaChannel *channel = tolchannel(L, arg);
	int ltype = lua_getiuservalue(L, arg, 1);
	luaL_argcheck(L, ltype == LUA_TSTRING, arg, "closed channel");
	lua_pop(L, 1);
	return channel;
}

static int channelclose (lua_State *L, LuaChannel *channel) {
	lcu_ChannelMap *map = lcuCS_tochannelmap(L);
	if (lua_getiuservalue(L, 1, 1) == LUA_TSTRING) {
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
		lua_pop(L, 1);  /* channel name */
		/* DEBUG: channel->sync = NULL; */
		/* DEBUG: channel->L = NULL; */
		/* DEBUG: channel->handle = NULL; */
		lua_pushnil(L);
		lua_setiuservalue(L, 1, 1);
		return 1;
	}
	return 0;
}

static int channelsync (lcu_ChannelSync *sync,
                        lua_State *L,
                        lcu_GetAsyncState getstate,
                        void *userdata) {
	int endpoint = lcuCS_checksyncargs(L, 2);
	if (endpoint == -1) lua_error(L);
	if (lcuCS_matchchsync(sync, endpoint, L, 1, lua_gettop(L), getstate, userdata)) {
		lcu_assert(lua_tointeger(L, -1) == lua_gettop(L)-2);
		lua_pop(L, 1);  /* discard 'narg' */
		return 1;
	}
	return 0;
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

/* res [, errmsg] = system.awaitch(channel, endpoint, ...) */
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
	int err, nret;
	lcu_assert(lua_tointeger(cL, -1) == lua_gettop(cL)-1);
	lua_pop(cL, 1);  /* discard 'narg' */
	nret = lua_gettop(cL);
	err = lcuL_movefrom(NULL, L, cL, nret, "");
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
	lua_pushvalue(L, 1);
	lcu_setopvalue(L);
	return 0;
}

static void uv_onsynced (uv_async_t *async) {
	uv_handle_t *handle = (uv_handle_t *)async;
	lua_State *thread = (lua_State *)handle->data;
	lcu_ChannelTask *channeltask;
	lua_getfield(thread, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
	channeltask = (lcu_ChannelTask *)lua_touserdata(thread, -1);
	lua_pop(thread, 1);
	if (channeltask) {
		uv_mutex_lock(&channeltask->mutex);
		channeltask->wakes--;
		uv_mutex_unlock(&channeltask->mutex);
	}
	if (lcuU_endcohdl(handle)) lcuU_resumecohdl(handle, 0);
	else {
		LuaChannel *canceled;
		lcu_pushopvalue(thread);
		canceled = (LuaChannel *)lua_touserdata(thread, -1);
		lua_pop(thread, 1);
		lua_pushnil(thread);
		lcu_setopvalue(thread);
		restorechannel(canceled);
	}
}

typedef struct ArmSyncedArgs {
	uv_loop_t *loop;
	uv_async_t *async;
	lcu_Operation *op;
	LuaChannel *channel;
} ArmSyncedArgs;

static lua_State *armsynced (lua_State *L, void *data) {
	ArmSyncedArgs *args = (ArmSyncedArgs *)data;
	lua_State *cL = args->channel->L;
	int top = lua_gettop(L);
	int narg = top > 2 ? top-2 : 0;
	lcu_assert(lua_gettop(cL) == 0);
	if (!lua_checkstack(cL, narg+3)) {  /* boolean, nargs, and two integers */
		lua_settop(L, 1);
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "insufficient memory");
		return NULL;
	} else {
		int err;
		if (narg > 0) {
			lua_settop(cL, 1);  /* placeholder for boolean first return */
			err = lcuL_movefrom(cL, cL, L, narg, "argument");
			if (err != LUA_OK) {
				lua_settop(L, 1);
				lua_pushboolean(L, 0);
				err = lcuL_pushfrom(L, L, cL, -1, "error");
				if (err != LUA_OK) lcuL_warnmsg(L, "discarded error", lua_tostring(cL, -1));
				lua_settop(cL, 0);
				return NULL;
			}
		}
		if (args->loop != NULL) {
			err = uv_async_init(args->loop, args->async, uv_onsynced);
			lcuT_armcohdl(L, args->op, err);
			if (err < 0) {
				lua_settop(L, 1);
				lcuL_pusherrres(L, err);
				lua_settop(cL, 0);
				return NULL;
			}
		}

		lua_pushlightuserdata(cL, args->async);
		lua_setfield(cL, LUA_REGISTRYINDEX, LCU_CHANNELSYNCREGKEY);
		lua_pushinteger(cL, 0);  /* base */
		lua_pushinteger(cL, narg);

		args->channel->handle = args->async;

		return cL;
	}
}

static int k_setupsynced (lua_State *L,
                          uv_handle_t *handle,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	ArmSyncedArgs args;
	LuaChannel *channel = chklchannel(L, 1);
	luaL_argcheck(L, channel->handle == NULL, 1, "in use");
	args.loop = loop;
	args.async = (uv_async_t *)handle;
	args.op = op;
	args.channel = channel;
	if (channelsync(channel->sync, L, armsynced, &args)) return lua_gettop(L)-1;
	return -1;
}

static int system_awaitch (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetcohdlk(L, UV_ASYNC, sched, k_setupsynced,
	                                            returnsynced,
	                                            cancelsynced);
}

/* res [, errmsg] = channel:sync(endpoint) */
static lua_State *cancelsuspension (lua_State *L, void *data) {
	lcu_assert(data == NULL);
	lua_settop(L, 1);
	lua_pushboolean(L, 0);
	lua_pushliteral(L, "empty");
	lua_pushinteger(L, 2);  /* push 'narg' */
	return NULL;
}

static int channel_sync (lua_State *L) {
	LuaChannel *channel = chklchannel(L, 1);
	channelsync(channel->sync, L, cancelsuspension, NULL);
	return lua_gettop(L)-1;
}

static int channel_getname (lua_State *L) {
	chklchannel(L, 1);
	lua_getiuservalue(L, 1, 1);
	return 1;
}

static int channeltask_gc (lua_State *L) {
	lcu_ChannelTask *channeltask = (lcu_ChannelTask *)lua_touserdata(L, 1);
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
	lcu_ChannelTask *channeltask;

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
	luaL_setmetatable(L, LCU_CHANNELCLS);

	if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TNIL) {
		if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_TASKTPOOLREGKEY) == LUA_TNIL) {
			channeltask = NULL;
			lua_pushboolean(L, 0);
		} else {
			int err;
			channeltask = (lcu_ChannelTask *)lua_newuserdatauv(L, sizeof(lcu_ChannelTask), 0);
			channeltask->wakes = 0;
			channeltask->L = NULL;
			err = uv_mutex_init(&channeltask->mutex);
			if (err) return lcuL_pusherrres(L, err);
			luaL_setmetatable(L, LCU_CHANNELTASKCLS);
		}
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
		lua_pop(L, 1);  /* LCU_TASKTPOOLREGKEY */
	} else {
		channeltask = (lcu_ChannelTask *)lua_touserdata(L, -1);
	}
	lua_pop(L, 1);  /* LCU_CHANNELTASKREGKEY */

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
	lcuCS_tochannelmap(L);  /* map shall be GC after any channel on Lua close */
	luaL_newlib(L, modf);
	luaL_newmetatable(L, LCU_CHANNELTASKCLS);
	luaL_setfuncs(L, channeltaskf, 0);  /* add metamethods to metatable */
	lua_pop(L, 1);  /* pop metatable */
	luaL_newmetatable(L, LCU_CHANNELCLS);
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
