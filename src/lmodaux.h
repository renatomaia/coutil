#ifndef lmodaux_h
#define lmodaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>


#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

#define lcu_pusherror(L,e)	lua_pushstring(L, uv_strerror(e))

LCULIB_API void lcu_chkerror (lua_State *L, int err);

LCULIB_API int lcuL_doresults (lua_State *L, int n, int err);


#define lcu_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(1))


#define LCU_MODUPVS	3

LCULIB_API void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv);

LCULIB_API void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup);

LCULIB_API void lcuM_newclass (lua_State *L, const luaL_Reg *l, int nup,
                               const char *name, const char *super);

LCULIB_API void lcuL_printstack (lua_State *L, const char *file, int line,
                                               const char *func);

#endif
