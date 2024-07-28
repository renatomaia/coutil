#ifndef lchaux_h
#define lchaux_h


#include "lcuconf.h"

#include <lua.h>


typedef struct lcu_StateQ {
	lua_State *head;
	lua_State *tail;
} lcu_StateQ;

LCUI_FUNC void lcuCS_initstateq (lcu_StateQ *q);

LCUI_FUNC int lcuCS_emptystateq (lcu_StateQ *q);

LCUI_FUNC void lcuCS_enqueuestateq (lcu_StateQ *q, lua_State *L);

LCUI_FUNC lua_State *lcuCS_dequeuestateq (lcu_StateQ *q);

LCUI_FUNC lua_State *lcuCS_removestateq (lcu_StateQ *q, lua_State *L);


LCUI_FUNC int lcuCS_checksyncargs (lua_State *L, int narg);

typedef struct lcu_ChannelSync lcu_ChannelSync;

#define LCU_CHSYNCIN	0x01
#define LCU_CHSYNCOUT	0x02
#define LCU_CHSYNCANY	(LCU_CHSYNCIN|LCU_CHSYNCOUT)

typedef lua_State *(*lcu_GetAsyncState) (lua_State *L, void *userdata);

LCUI_FUNC int lcuCS_matchchsync (lcu_ChannelSync *sync,
                                 int endpoint,
                                 lua_State *L,
                                 int base,
                                 int narg,
                                 lcu_GetAsyncState getstate,
                                 void *userdata);


typedef struct lcu_ChannelMap lcu_ChannelMap;

LCUI_FUNC lcu_ChannelMap *lcuCS_tochannelmap (lua_State *L);

LCUI_FUNC lcu_ChannelSync *lcuCS_getchsync (lcu_ChannelMap *map,
                                            const char *name);

LCUI_FUNC void lcuCS_freechsync (lcu_ChannelMap *map, const char *name);


#define LCU_CHANNELTASKCLS	LCU_PREFIX"lcu_ChannelTask"

#define LCU_CHANNELSYNCREGKEY	LCU_PREFIX"uv_async_t channelWake"

LCUI_FUNC int lcuCS_suspendedchtask (lua_State *L, int idx);


#endif
