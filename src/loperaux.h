#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


#define LCU_MODUPVS	1

/* scheduler operations */

LCUI_FUNC void lcuM_newmodupvs (lua_State *L);

typedef struct lcu_Scheduler lcu_Scheduler;

#define lcu_getsched(L)	(lcu_Scheduler *)lua_touserdata(L, lua_upvalueindex(1))

#define lcu_tosched(U)	((lcu_Scheduler *)U)

LCUI_FUNC uv_loop_t *lcu_toloop (lcu_Scheduler *sched);

LCUI_FUNC int lcu_shallsuspend (lcu_Scheduler *sched);

LCUI_FUNC void lcuU_checksuspend (uv_loop_t *loop);

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

#define LCU_OBJCLOSEDFLAG	0x01

typedef int (*lcu_ObjectAction) (uv_handle_t *h);

typedef struct lcu_Object {
	int flags;
	lcu_ObjectAction stop;
	lua_CFunction step;
	uv_handle_t handle;
} lcu_Object;

#define lcu_isobjclosed(O)	((O)->flags&LCU_OBJCLOSEDFLAG)

#define lcu_toobjhdl(O)	(&(O)->handle)

#define lcu_tohdlobj(H) ({ const char *p = (const char *)H; \
                           (lcu_Object *)(p-offsetof(lcu_Object, H)); })

LCUI_FUNC lcu_Object *lcuT_createobj (lua_State *L, size_t sz, const char *cls);

LCUI_FUNC int lcu_closeobj (lua_State *L, int idx);

LCUI_FUNC int lcuT_resetobjopk (lua_State *L,
                                lcu_Object *obj,
                                lcu_ObjectAction start,
                                lcu_ObjectAction stop,
                                lua_CFunction step);

LCUI_FUNC void lcuU_resumeobjop (uv_handle_t *handle, int narg);


#endif