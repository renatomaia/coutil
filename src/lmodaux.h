#ifndef lmodaux_h
#define lmodaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>


#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

#define lcu_pusherror(L,e)	lua_pushstring(L, uv_strerror(e))

LCUI_FUNC int lcuL_pusherrres (lua_State *L, int err);

LCUI_FUNC int lcuL_pushresults (lua_State *L, int n, int err);

#define lcuL_maskflag(O,F) ((O)->flags&(F))
#define lcuL_setflag(O,F) ((O)->flags |= (F))
#define lcuL_clearflag(O,F) ((O)->flags &= ~(F))

LCUI_FUNC lua_State *lcuL_newstate (lua_State *L);

typedef int (*lcuL_CustomTransfer) (lua_State *from, lua_State *to, int arg);

LCUI_FUNC int lcuL_pushargfrom (lua_State *to,
                                lua_State *from,
                                int arg,
                                lcuL_CustomTransfer customf);

LCUI_FUNC int lcuL_moveargsfrom (lua_State *to,
                                 lua_State *from,
                                 int narg,
                                 lcuL_CustomTransfer customf);


#define LCU_MODUPVS	3

#define lcu_toloop(L)	(uv_loop_t *)lua_touserdata(L, lua_upvalueindex(3))

LCUI_FUNC void lcuM_newmodupvs (lua_State *L, uv_loop_t *uv);

LCUI_FUNC void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup);

LCUI_FUNC void lcuM_newclass (lua_State *L, const char *name);

LCUI_FUNC void lcuL_printstack (lua_State *L, const char *file, int line,
                                              const char *func);

#endif
