#ifndef looplib_h
#define looplib_h


#include <lua.h>
#include <lauxlib.h>


#ifndef LOOPLIB_API
#define LOOPLIB_API
#endif


LOOPLIB_API void loop_setmethods (lua_State *L, const luaL_Reg *meth, int nup);

LOOPLIB_API void loop_makesubclass (lua_State *L, int superidx);

LOOPLIB_API int loop_issubclass (lua_State *L, int superidx);

LOOPLIB_API void loopL_newclass (lua_State *L, const char *name,
                                               const char *super);

LOOPLIB_API void *loopL_testinstance (lua_State *L, int idx, const char *cls);

LOOPLIB_API void *loopL_checkinstance (lua_State *L, int idx, const char *cls);


#endif
