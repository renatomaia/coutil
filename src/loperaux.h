#ifndef loperaux_h
#define loperaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>


typedef struct lcu_PendingOp {
	union {
		union uv_any_handle handle;
		uv_handle_t *object;
		struct {
			union uv_any_req value;
			uv_loop_t *loop;
		} request;
	} kind;
	int flags;
} lcu_PendingOp;


#define LCU_OPTYPE_GLOBAL  0x00
#define LCU_OPTYPE_OBJECT  0x01
#define LCU_OPTYPE_REQUEST  0x02

#define LCU_OPFLAG_TYPE  0x03
#define LCU_OPFLAG_PENDING  0x04

#define lcu_getoptype(O)  (O->flags&LCU_OPFLAG_TYPE)
#define lcu_ispendingop(O)  (O->flags&LCU_OPFLAG_PENDING)

#define lcu_toglobalop(O)  ((uv_handle_t *)&((O)->kind.handle))
#define lcu_toobjectop(O)  ((O)->kind.object)
#define lcu_torequestop(O)  ((uv_req_t *)&((O)->kind.request.value))

LCULIB_API lcu_PendingOp *lcu_getopof (lua_State *L);

LCULIB_API void lcu_chkinitop (lua_State *L, lcu_PendingOp *op,
                               uv_loop_t *loop, int err);

LCULIB_API void lcu_freereq (lcu_PendingOp *op);

LCULIB_API void lcu_chkstarthdl (lua_State *L, uv_handle_t *h, int err);


LCULIB_API int lcu_yieldop (lua_State *L, lua_KContext ctx, lua_KFunction func,
                            lcu_PendingOp *op);

LCULIB_API void lcu_resumeop (lcu_PendingOp *op, lua_State *co);

LCULIB_API lcu_PendingOp *lcu_resetop (lua_State *L, int req, int type,
                                       lua_KContext ctx, lua_KFunction func);

#define lcu_resethdl(L,T,C,F)  lcu_resetop(L, 0, T, C, F)
#define lcu_resetreq(L,T,C,F)  lcu_resetop(L, 1, T, C, F)

LCULIB_API int lcu_doresumed (lua_State *L, uv_loop_t *loop, lcu_PendingOp *op);

LCULIB_API int lcuK_chkignoreop (lua_State *L, int status, lua_KContext ctx);


#endif