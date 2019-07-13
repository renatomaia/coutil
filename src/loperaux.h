#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


/* request operations */

typedef int (*lcu_RequestSetup) (lua_State *L, uv_req_t *r, uv_loop_t *l);

LCULIB_API int lcuT_resetreqopk (lua_State *L,
                                 lcu_RequestSetup setup,
                                 lua_CFunction results);

LCULIB_API lua_State *lcuU_endreqop (uv_loop_t *loop, uv_req_t *request);

LCULIB_API void lcuU_resumereqop (lua_State *thread,
                                  uv_loop_t *loop,
                                  uv_req_t *request);

/* thread operations */

typedef int (*lcu_HandleSetup) (lua_State *L, uv_handle_t *h, uv_loop_t *l);

LCULIB_API int lcuT_resetthropk (lua_State *L,
                                 uv_handle_type type,
                                 lcu_HandleSetup setup,
                                 lua_CFunction results);

LCULIB_API int lcuT_armthrop (lua_State *L, int err);

LCULIB_API int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle);

/* object operations */

LCULIB_API void lcu_closeobj (lua_State *L, int idx, uv_handle_t *handle);

LCULIB_API void lcuT_awaitobj (lua_State *L, uv_handle_t *handle);

LCULIB_API int lcuT_haltedobjop (lua_State *L, uv_handle_t *handle);

LCULIB_API int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle);






#endif