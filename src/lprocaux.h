#ifndef lprocaux_h
#define lprocaux_h


#include "lcuconf.h"

#include <lua.h>
#include <lauxlib.h>


#define LCU_PROCESSCLS LCU_PREFIX"process"

typedef struct lcu_Process lcu_Process;

#define lcu_checkprocess(L,i)	((lcu_Process *) \
                             	 luaL_checkudata(L, i, LCU_PROCESSCLS))

#define lcu_toprocess(L,i)	((lcu_Process *) \
                          	 luaL_testudata(L, i, LCU_PROCESSCLS))

#define lcu_isaddress(L,i)  (lcu_toprocess(L, i) != NULL)

LCULIB_API lcu_Process *lcu_newprocess (lua_State *L);

LCULIB_API int lcu_isprocclosed (lcu_Process *process);

LCULIB_API int lcu_closeproc (lua_State *L, int idx);

LCULIB_API void lcu_enableproc (lua_State *L, int idx);

LCULIB_API uv_process_t *lcu_toprochandle (lcu_Process *process);

LCULIB_API int lcu_getprocexited (lcu_Process *process,
                                  int64_t *exitval,
                                  int *signal);

LCULIB_API void lcu_setprocexited (lcu_Process *process,
                                   int64_t exitval,
                                   int signal);


#endif
