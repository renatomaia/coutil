#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


#define lcu_testflag(V,B)  ((V)->flags&(B))
#define lcu_setflag(V,B)  ((V)->flags |= (B))
#define lcu_clearflag(V,B)  ((V)->flags &= ~(F))

/* thread operations */

#define LCU_OPFLAG_REQUEST  0x01
#define LCU_OPFLAG_PENDING  0x02

typedef struct lcu_Operation {
	union {
		union uv_any_req request;
		union uv_any_handle handle;
	} kind;
	int flags;
	lua_CFunction results;
} lcu_Operation;

#define lcu_ispending(O) ((O)->results)
#define lcu_isrequest(O) lcuL_testflag(O, LCU_OPFLAG_REQUEST)
#define lcu_torequest(O) ((uv_req_t *)&((O)->kind.request))
#define lcu_tohandle(O) ((uv_handle_t *)&((O)->kind.handle))

LCULIB_API void lcu_freethrop (lcu_Operation *op);

LCULIB_API void lcuL_checkthrop (lua_State *L, lcu_Operation *op, int err);

LCULIB_API lcu_Operation *lcuT_tothrop (lua_State *L);

LCULIB_API int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle);

LCULIB_API int lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int err);

LCULIB_API int lcuT_doneop (lua_State *L, uv_loop_t *loop);

LCULIB_API int lcuK_donethrop (lua_State *L,
                               int status,
                               lua_KContext kctx);

LCULIB_API void lcuT_armthrop (lua_State *L, lcu_Operation *op);

LCULIB_API int lcuT_awaitopk (lua_State *L,
                              lcu_Operation *op,
                              lua_KContext kctx,
                              lua_KFunction kfn);

LCULIB_API lcu_Operation *lcuT_resetopk (lua_State *L,
                                         int request,
                                         int type,
                                         lua_KContext kctx,
                                         lua_KFunction setup);

/* object operations */

LCULIB_API void lcu_closeobj (lua_State *L, int idx, uv_handle_t *handle);

LCULIB_API void lcu_releaseobj (lua_State *L, uv_handle_t *handle);

LCULIB_API int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle);

LCULIB_API int lcuT_awaitobjk (lua_State *L,
                               uv_handle_t *handle,
                               lua_KContext kctx,
                               lua_KFunction kfn);


#endif