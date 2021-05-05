#ifndef lttyaux_h
#define lttyaux_h


#include "lcuconf.h"

#include <lua.h>


#define LCU_STDIOFDCOUNT  3

#ifdef LCU_ENABLESTDIODUP
LCUI_FUNC int *lcuTY_tostdiofd (lua_State *L);
#else
static int lcuTY_stdiofd[3] = { 0, 1, 2 };
#define lcuTY_tostdiofd(L) (lcuTY_stdiofd)
#endif


#endif
