#include "lmodaux.h"

LCUI_FUNC void lcuM_addscheduf (lua_State *L);

LCUI_FUNC void lcuM_addtimef (lua_State *L);

LCUI_FUNC void lcuM_addsignalf (lua_State *L);

LCUI_FUNC void lcuM_addcommunc (lua_State *L);
LCUI_FUNC void lcuM_addcommunf (lua_State *L);

LCUI_FUNC void lcuM_addcoroutc (lua_State *L);
LCUI_FUNC void lcuM_addcoroutf (lua_State *L);

LCUI_FUNC void lcuM_addthreadf (lua_State *L);

LCUMOD_API int luaopen_coutil_system (lua_State *L) {
	lcuM_newmodupvs(L, NULL);

	lcuM_addcommunc(L);
	lcuM_addcoroutc(L);

	lua_newtable(L);
	lcuM_addtimef(L);
	lcuM_addsignalf(L);
	lcuM_addcommunf(L);
	lcuM_addcoroutf(L);
	lcuM_addthreadf(L);
	lcuM_addscheduf(L);

	return 1;
}
