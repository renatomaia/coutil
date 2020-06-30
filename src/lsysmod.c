#include "lmodaux.h"

LCULIB_API void lcuM_addscheduf (lua_State *L);

LCULIB_API void lcuM_addtimef (lua_State *L);

LCULIB_API void lcuM_addsignalf (lua_State *L);

LCULIB_API void lcuM_addnetadrf (lua_State *L);


LUAMOD_API int luaopen_coutil_system (lua_State *L) {
	lua_newtable(L);
	lcuM_newmodupvs(L, NULL);
	lcuM_addscheduf(L);
	lcuM_addtimef(L);
	lcuM_addsignalf(L);
	lcuM_addnetadrf(L);
	lua_pop(L, LCU_MODUPVS);
	return 1;
}
