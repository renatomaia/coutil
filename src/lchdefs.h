#ifndef lchdefs_h
#define lchdefs_h


#include "lchaux.h"

#include <lua.h>
#include <uv.h>


struct lcu_ChannelSync {
	uv_mutex_t mutex;
	int refcount;
	int expected;
	lcu_StateQ queue;
};

struct lcu_ChannelMap {
	uv_mutex_t mutex;
	lua_State *L;
};

typedef struct lcu_ChannelTask {
	uv_mutex_t mutex;
	int wakes;
	lua_State *L;
} lcu_ChannelTask;


#endif
