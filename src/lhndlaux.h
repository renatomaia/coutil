#ifndef lhndlaux_h
#define lhndlaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


LCULIB_API uv_handle_t *lcu_tohandle (lua_State *L);

LCULIB_API void lcu_chkinithdl (lua_State *L, uv_handle_t *h, int err);

LCULIB_API void lcu_chkstarthdl (lua_State *L, uv_handle_t *h, int err);


#define lcu_yieldhdl(L,n,c,f,h) ((h)->data = L, lua_yieldk(L, n, c, f))

LCULIB_API void lcu_resumehdl (uv_handle_t *h, lua_State *co);

LCULIB_API uv_handle_t *lcu_resethdl (lua_State *L, uv_handle_type t, int narg,
                                      lua_KContext ctx, lua_KFunction func);

LCULIB_API int lcuK_chkcancelhdl (lua_State *L, int status, lua_KContext ctx);


#endif