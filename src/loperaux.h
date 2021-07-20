#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


#define LCU_LIBUVMINVER(minor)  (UV_VERSION_HEX >= (0x10000 | (minor)<<8))

#define LCU_MODUPVS	1
#define LCU_NOYIELDMODE '~'

/* scheduler operations */

LCUI_FUNC void lcuM_newmodupvs (lua_State *L);

typedef struct lcu_Scheduler lcu_Scheduler;

typedef struct lcu_Operation lcu_Operation;

#define lcu_getsched(L)	(lcu_Scheduler *)lua_touserdata(L, lua_upvalueindex(1))

#define lcu_tosched(U)	((lcu_Scheduler *)U)

LCUI_FUNC uv_loop_t *lcu_toloop (lcu_Scheduler *sched);

LCUI_FUNC int lcu_shallsuspend (lcu_Scheduler *sched);

LCUI_FUNC void lcuU_checksuspend (uv_loop_t *loop);

LCUI_FUNC void lcu_setopvalue (lua_State *L);

LCUI_FUNC int lcu_pushopvalue (lua_State *L);

/* request operations */

typedef int (*lcu_RequestSetup) (lua_State *L,
                                 uv_req_t *request,
                                 uv_loop_t *loop,
                                 lcu_Operation *op);

LCUI_FUNC int lcuT_resetcoreqk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel);

LCUI_FUNC void lcuT_armcoreq (lua_State *L,
                              uv_loop_t *loop,
                              lcu_Operation *op,
                              int err);

LCUI_FUNC lua_State *lcuU_endcoreq (uv_loop_t *loop, uv_req_t *request);

LCUI_FUNC void lcuU_resumecoreq (uv_loop_t *loop, uv_req_t *request, int narg);

/* thread operations */

typedef int (*lcu_HandleSetup) (lua_State *L,
                                uv_handle_t *handle,
                                uv_loop_t *loop,
                                lcu_Operation *op);

LCUI_FUNC int lcuT_resetcohdlk (lua_State *L,
                                uv_handle_type type,
                                lcu_Scheduler *sched,
                                lcu_HandleSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel);

LCUI_FUNC int lcuT_armcohdl (lua_State *L, lcu_Operation *op, int err);

LCUI_FUNC int lcuU_endcohdl (uv_handle_t *handle);

LCUI_FUNC void lcuU_resumecohdl (uv_handle_t *handle, int narg);

/* object operations */

#define LCU_HANDLECLOSEDFLAG	0x01

typedef int (*lcu_HandleAction) (uv_handle_t *h);

typedef struct lcu_UdataHandle {
	int flags;
	lcu_HandleAction stop;
	lua_CFunction step;
	uv_handle_t handle;
} lcu_UdataHandle;

#define lcu_ud2hdl(O)	(&(O)->handle)

#define lcu_hdl2ud(H) ((lcu_UdataHandle *)(((const char *)H)-offsetof(lcu_UdataHandle, H)))

LCUI_FUNC lcu_UdataHandle *lcuT_createudhdl (lua_State *L,
                                             int schedidx,
                                             size_t sz,
                                             const char *cls);

#define lcuT_newudhdl(L,T,C)	(T *)lcuT_createudhdl(L,lua_upvalueindex(1),sizeof(T),C)

LCUI_FUNC int lcu_closeudhdl (lua_State *L, int idx);

LCUI_FUNC lcu_UdataHandle *lcu_openedudhdl (lua_State *L, int arg, const char *class);

LCUI_FUNC int lcuT_resetudhdlk (lua_State *L,
                                lcu_UdataHandle *obj,
                                lcu_HandleAction start,
                                lcu_HandleAction stop,
                                lua_CFunction step);

LCUI_FUNC void lcuU_resumeudhdl (uv_handle_t *handle, int narg);



typedef struct lcu_UdataRequest {
	lua_CFunction results;
	lua_CFunction cancel;
	uv_req_t request;
} lcu_UdataRequest;

#define lcu_ud2req(O)	(&(O)->request)

#define lcu_req2ud(H) ((lcu_UdataRequest *)(((const char *)H)-offsetof(lcu_UdataRequest, H)))

LCUI_FUNC lcu_UdataRequest *lcuT_createudreq (lua_State *L, size_t sz);

#define lcuT_newudreq(L,T)	(T *)lcuT_createudreq(L,sizeof(T))

LCUI_FUNC int lcuT_resetudreqk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_UdataRequest *objreq,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel);

LCUI_FUNC lua_State *lcuU_endudreq (uv_loop_t *loop, uv_req_t *request);

LCUI_FUNC void lcuU_resumeudreq (uv_loop_t *loop, uv_req_t *request, int narg);



/* auxiliary functions */

LCUI_FUNC int lcuL_checknoyieldmode (lua_State *L, int arg);


#endif