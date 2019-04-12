#ifndef lhndlaux_h
#define lhndlaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


#define LCU_COREGISTRY	lua_upvalueindex(2)
#define LCU_HANDLEMAP	lua_upvalueindex(3)


LCULIB_API uv_handle_t *lcu_tohandle (lua_State *L);

LCULIB_API void lcu_checkinit (lua_State *L, uv_handle_t *h, int err);

LCULIB_API void lcu_checkstart (lua_State *L, uv_handle_t *h, int err);


#define lcu_yieldk(L,n,c,f,h) ((h)->data = L, lua_yieldk(L, n, c, f))

LCULIB_API void lcu_resumehandle (uv_handle_t *h, lua_State *co);

LCULIB_API uv_handle_t *lcu_resethandle (lua_State *L, uv_handle_type t, int narg,
                                         lua_KContext ctx, lua_KFunction func);

LCULIB_API int lcuK_checkcancel (lua_State *L, int status, lua_KContext ctx);


#endif