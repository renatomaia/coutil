#ifndef lmodaux_h
#define lmodaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>


#if !defined(lcu_assert)
#define lcu_assert(X)	((void)(X))
#endif


#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

LCULIB_API void lcu_checkerr (lua_State *L, int err);


#define lcu_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(1))


#define LCU_MODUPVS	3

LCULIB_API void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv);

LCULIB_API void lcuM_addmodfunc (lua_State *L, const luaL_Reg *l);


#endif