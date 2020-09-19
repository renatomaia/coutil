#ifndef lthpool_h
#define lthpool_h


#include "lcuconf.h"

#include <lua.h>


typedef struct lcu_ThreadPool lcu_ThreadPool;

LCUI_FUNC int lcuTP_createtpool (lcu_ThreadPool **ref,
                                 lua_Alloc allocf,
                                 void *allocud);

LCUI_FUNC void lcuTP_destroytpool (lcu_ThreadPool *pool);

LCUI_FUNC void lcuTP_closetpool (lcu_ThreadPool *pool);

LCUI_FUNC int lcuTP_resizetpool (lcu_ThreadPool *pool, int size, int create);

LCUI_FUNC int lcuTP_addtpooltask (lcu_ThreadPool *pool, lua_State *L);

typedef struct lcu_ThreadCount {
	int expected;
	int actual;
	int running;
	int pending;
	int suspended;
	int numoftasks;
} lcu_ThreadCount;

LCUI_FUNC int lcuTP_counttpool (lcu_ThreadPool *pool,
                                lcu_ThreadCount *count,
                                const char *what);


LCUI_FUNC void lcuTP_resumetask (lua_State *L);


#endif
