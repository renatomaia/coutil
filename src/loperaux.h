#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


/* request operations */

typedef int (*lcu_RequestSetup) (lua_State *L, uv_req_t *r, uv_loop_t *l);

LCUI_FUNC int lcuT_resetreqopk (lua_State *L,
                                lcu_RequestSetup setup,
                                lua_CFunction results);

LCUI_FUNC lua_State *lcuU_endreqop (uv_loop_t *loop, uv_req_t *request);

LCUI_FUNC void lcuU_resumereqop (lua_State *thread,
                                 uv_loop_t *loop,
                                 uv_req_t *request);

LCUI_FUNC void lcuU_completereqop (uv_loop_t *loop,
                                   uv_req_t *request,
                                   int err);

/* thread operations */

typedef int (*lcu_HandleSetup) (lua_State *L, uv_handle_t *h, uv_loop_t *l);

LCUI_FUNC int lcuT_resetthropk (lua_State *L,
                                uv_handle_type type,
                                lcu_HandleSetup setup,
                                lua_CFunction results);

LCUI_FUNC int lcuT_armthrop (lua_State *L, int err);

LCUI_FUNC int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle);

/* object operations */

LCUI_FUNC void lcu_closeobjhdl (lua_State *L, int idx, uv_handle_t *handle);

LCUI_FUNC void lcuT_awaitobj (lua_State *L, uv_handle_t *handle);

LCUI_FUNC int lcuT_haltedobjop (lua_State *L, uv_handle_t *handle);

LCUI_FUNC int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle);


#endif