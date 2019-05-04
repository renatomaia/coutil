#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


#define LCU_THROP 0
#define LCU_REQOP 1

LCULIB_API int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle);

LCULIB_API void lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int err);

LCULIB_API int lcuT_resetopk (lua_State *L,
                              int mkreq,
                              int kind,
                              void *setup,
                              lua_CFunction results);

LCULIB_API int lcuT_armthrop (lua_State *L, int err)

/* object operations */

LCULIB_API void lcu_closeobj (lua_State *L, int idx, uv_handle_t *handle);

LCULIB_API void lcu_releaseobj (lua_State *L, uv_handle_t *handle);

LCULIB_API int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle);

LCULIB_API int lcuT_awaitobjk (lua_State *L,
                               uv_handle_t *handle,
                               lua_KContext kctx,
                               lua_KFunction kfn);


#endif