#include "loperaux.h"
#include "lchaux.h"

LCUI_FUNC void lcuM_addchanelf (lua_State *L);
LCUI_FUNC void lcuM_addcommunf (lua_State *L);
LCUI_FUNC void lcuM_addcoroutf (lua_State *L);
LCUI_FUNC void lcuM_addfilef (lua_State *L);
LCUI_FUNC void lcuM_addinfof (lua_State *L);
LCUI_FUNC void lcuM_addprocesf (lua_State *L);
LCUI_FUNC void lcuM_addscheduf (lua_State *L);
LCUI_FUNC void lcuM_addtimef (lua_State *L);

LCUMOD_API int luaopen_coutil_system (lua_State *L) {
	lcuCS_tochannelmap(L);  /* map must be GC after 'sched' on 'lua_close' */
	lcuM_newmodupvs(L);
	lua_newtable(L);
	lcuM_addchanelf(L);
	lcuM_addcommunf(L);
	lcuM_addcoroutf(L);
	lcuM_addfilef(L);
	lcuM_addinfof(L);
	lcuM_addprocesf(L);
	lcuM_addscheduf(L);
	lcuM_addtimef(L);
	return 1;
}
