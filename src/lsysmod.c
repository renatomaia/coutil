#include "lmodaux.h"

LCULIB_API void lcuM_addscheduf (lua_State *L);

LCULIB_API void lcuM_addtimef (lua_State *L);

LCULIB_API void lcuM_addsignalf (lua_State *L);

LCULIB_API void lcuM_addipcf (lua_State *L);

LCULIB_API void lcuM_addipccls (lua_State *L);

LUAMOD_API int luaopen_coutil_system (lua_State *L) {
	lcuM_newmodupvs(L, NULL);
	lcuM_addipccls(L);
	lua_newtable(L);
	lcuM_addtimef(L);
	lcuM_addsignalf(L);
	lcuM_addipcf(L);
	lcuM_addscheduf(L);
	return 1;
}
