#include "loperaux.h"
#include "lchaux.h"

LCUI_FUNC void lcuM_addscheduf (lua_State *L);
LCUI_FUNC void lcuM_addtimef (lua_State *L);
LCUI_FUNC void lcuM_addfilef (lua_State *L);
LCUI_FUNC void lcuM_addsignalf (lua_State *L);
LCUI_FUNC void lcuM_addcommunf (lua_State *L);
LCUI_FUNC void lcuM_addsysinff (lua_State *L);
LCUI_FUNC void lcuM_addcoroutf (lua_State *L);
LCUI_FUNC void lcuM_addchanelf (lua_State *L);

LCUMOD_API int luaopen_coutil_system (lua_State *L) {
	lcuCS_tochannelmap(L);  /* map shall be GC after 'sched' on 'lua_close' */
	lcuM_newmodupvs(L);
	lua_newtable(L);
	lcuM_addtimef(L);
	lcuM_addfilef(L);
	lcuM_addsignalf(L);
	lcuM_addcommunf(L);
	lcuM_addsysinff(L);
	lcuM_addcoroutf(L);
	lcuM_addchanelf(L);
	lcuM_addscheduf(L);
	return 1;
}
