#ifndef lmodaux_h
#define lmodaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>


#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

#define lcu_pusherror(L,e)	lua_pushstring(L, uv_strerror(e))

LCULIB_API void *lcuL_allocmemo (lua_State *L, size_t size);

LCULIB_API void lcuL_freememo (lua_State *L, void *memo, size_t size);

LCULIB_API int lcuL_pushresults (lua_State *L, int n, int err);

#define lcuL_maskflag(O,F) ((O)->flags&(F))
#define lcuL_setflag(O,F) ((O)->flags |= (F))
#define lcuL_clearflag(O,F) ((O)->flags &= ~(F))


#define LCU_MODUPVS	3

#define lcu_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(1))

LCULIB_API void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv);

LCULIB_API void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup);

LCULIB_API void lcuM_newclass (lua_State *L, const char *name);

LCULIB_API void lcuL_printstack (lua_State *L, const char *file, int line,
                                               const char *func);

#endif
