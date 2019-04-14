#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


typedef struct lcu_PendingOp {
	union {
		union uv_any_handle handle;
		struct {
			union uv_any_req value;
			uv_loop_t *loop;
		} request;
	} kind;
	int flags;
} lcu_PendingOp;

#define LCU_OPFLAG_REQUEST  0x01
#define LCU_OPFLAG_PENDING  0x02

#define lcu_isrequestop(O)  (O->flags&LCU_OPFLAG_REQUEST)
#define lcu_ispendingop(O)  (O->flags&LCU_OPFLAG_PENDING)

#define lcu_tohandle(O) ((uv_handle_t *)&((O)->kind.handle))
#define lcu_torequest(O) ((uv_req_t *)&((O)->kind.request.value))

#define lcu_getopcoro(O) (lua_State *)(lcu_isrequestop(O) \
                                       ? lcu_torequest(O)->data \
                                       : lcu_tohandle(O)->data)

#define lcu_getoploop(O) (uv_loop_t *)(lcu_isrequestop(O) \
                                       ? (O)->kind.request.loop \
                                       : lcu_tohandle(O)->loop)

LCULIB_API lcu_PendingOp *lcu_getopof (lua_State *L);

LCULIB_API void lcu_chkinitop (lua_State *L, lcu_PendingOp *op,
                               uv_loop_t *loop, int err);

LCULIB_API void lcu_chkstarthdl (lua_State *L, uv_handle_t *h, int err);


LCULIB_API int lcu_yieldop (lua_State *L, int narg,
                            lua_KContext ctx, lua_KFunction func,
                            lcu_PendingOp *op);

LCULIB_API void lcu_resumeop (lcu_PendingOp *op, lua_State *co);

LCULIB_API lcu_PendingOp *lcu_resetop (lua_State *L, int req, int type, int narg,
                                       lua_KContext ctx, lua_KFunction func);

LCULIB_API int lcuK_chkignoreop (lua_State *L, int status, lua_KContext ctx);


#endif