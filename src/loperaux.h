#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"
#include "lsyslib.h"

#include <uv.h>
#include <lua.h>


#define LCU_MODUPVS	1

typedef struct lcu_Scheduler {
	uv_loop_t loop;
	int nasync;  /* number of active 'uv_async_t' handles */
	int nactive;  /* number of other active handles */
} lcu_Scheduler;

#define lcu_getsched(L)	(lcu_Scheduler *)lua_touserdata(L, lua_upvalueindex(1))

#define lcu_toloop(S)	(&((S)->loop))

#define lcu_tosched(U)	((lcu_Scheduler *)U)

LCUI_FUNC void lcuM_newmodupvs (lua_State *L);

LCUI_FUNC void lcuT_savevalue (lua_State *L, void *key);

LCUI_FUNC void lcuT_freevalue (lua_State *L, void *key);

LCUI_FUNC void lcuU_checksuspend(uv_loop_t *loop);

/* request operations */

typedef int (*lcu_RequestSetup) (lua_State *L, uv_req_t *r, uv_loop_t *l);

LCUI_FUNC int lcuT_resetreqopk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel);

LCUI_FUNC lua_State *lcuU_endreqop (uv_loop_t *loop, uv_req_t *request);

LCUI_FUNC void lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int narg);

/* thread operations */

typedef int (*lcu_HandleSetup) (lua_State *L, uv_handle_t *h, uv_loop_t *l);

LCUI_FUNC int lcuT_resetthropk (lua_State *L,
                                uv_handle_type type,
                                lcu_Scheduler *sched,
                                lcu_HandleSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel);

LCUI_FUNC int lcuT_armthrop (lua_State *L, int err);

LCUI_FUNC int lcuU_endthrop (uv_handle_t *handle);

LCUI_FUNC void lcuU_resumethrop (uv_handle_t *handle, int narg);

/* object operations */

LCUI_FUNC void lcuT_closeobjhdl (lua_State *L, int idx, uv_handle_t *handle);

LCUI_FUNC int lcuT_resetobjopk (lua_State *L,
                                lcu_Object *obj,
                                lcu_ObjectAction start,
                                lcu_ObjectAction stop,
                                lua_CFunction step);

LCUI_FUNC void lcuU_resumeobjop (uv_handle_t *handle, int narg);


#endif