#include "lmodaux.h"
#include "loperaux.h"


#define LCU_COREGISTRY	lua_upvalueindex(2)
#define LCU_OPERATIONS	lua_upvalueindex(3)


LCULIB_API lcu_PendingOp *lcu_getopof (lua_State *L) {
	lcu_PendingOp *op;
	lua_pushthread(L);
	if (lua_gettable(L, LCU_OPERATIONS) == LUA_TNIL) {
		lua_pushthread(L);
		op = (lcu_PendingOp *)lua_newuserdata(L, sizeof(lcu_PendingOp));
		op->flags = 0;
		lcu_tohandle(op)->type = UV_UNKNOWN_HANDLE;
		lua_settable(L, LCU_OPERATIONS);
	}
	else op = (lcu_PendingOp *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return op;
}


static void saveopcoro (lcu_PendingOp *op, lua_State *L) {
	lua_pushlightuserdata(L, op);
	lua_pushthread(L);
	lua_settable(L, LCU_COREGISTRY);
}

static void freeopcoro (lcu_PendingOp *op) {
	lua_State *L = (lua_State *)lcu_getoploop(op)->data;
	lua_pushlightuserdata(L, op);
	lua_pushnil(L);
	lua_settable(L, LCU_COREGISTRY);
}

LCULIB_API void lcu_chkinitop (lua_State *L, lcu_PendingOp *op,
                               uv_loop_t * loop, int err) {
	lcu_chkerror(L, err);
	if (lcu_isrequestop(op)) op->kind.request.loop = loop;
	saveopcoro(op, L);
}

static void lcuB_onhandleclosed (uv_handle_t *handle) {
	handle->type = UV_UNKNOWN_HANDLE;
	lua_State *co = (lua_State *)handle->data;
	lcu_PendingOp *op = (lcu_PendingOp *)handle;
	if (lcu_ispendingop(op)) lcu_resumeop(op, co);
	else freeopcoro(op);
}

LCULIB_API void lcu_chkstarthdl (lua_State *L, uv_handle_t *h, int err) {
	if (err < 0) {
		uv_close(h, lcuB_onhandleclosed);
		lcu_error(L, err);  /* never returns */
	}
}


#define setpendingop(O)  ((O)->flags |= LCU_OPFLAG_PENDING)
#define setignoredop(O)  ((O)->flags &= ~LCU_OPFLAG_PENDING)
#define setrequestop(O)  ((O)->flags |= LCU_OPFLAG_REQUEST)
#define sethandleop(O)  ((O)->flags &= ~LCU_OPFLAG_REQUEST)


LCULIB_API int lcu_yieldop (lua_State *L, int narg,
                            lua_KContext ctx, lua_KFunction func,
                            lcu_PendingOp *op) {
	if (lcu_isrequestop(op)) lcu_torequest(op)->data = (void *)L;
	else lcu_tohandle(op)->data = (void *)L;
	setpendingop(op);
	return lua_yieldk(L, narg, ctx, func);
}

LCULIB_API void lcu_resumeop (lcu_PendingOp *op, lua_State *co) {
	uv_loop_t *loop = lcu_getoploop(op);
	lua_State *L = (lua_State *)loop->data;
	int status;
	setignoredop(op);  /* coroutine not interested anymore */
	lua_pushlightuserdata(co, loop);  /* token to sign scheduler resume */
	status = lua_resume(co, L, 1);
	if (!lcu_isrequestop(op) && (status != LUA_YIELD || !lcu_ispendingop(op)) ) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (handle->type != UV_UNKNOWN_HANDLE && !uv_is_closing(handle))
			uv_close(handle, lcuB_onhandleclosed);
	}
}

LCULIB_API lcu_PendingOp *lcu_resetop (lua_State *L, int req, int type, int narg,
                                       lua_KContext ctx, lua_KFunction func) {
	lcu_PendingOp *op = lcu_getopof(L);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (!lcu_isrequestop(op)) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (handle->type == UV_UNKNOWN_HANDLE) {
			if (req) setrequestop(op);
			func(L, LUA_YIELD, ctx);  /* never returns */
		} else if (!uv_is_closing(handle)) {
			if (!req && handle->type == type) return op;
			uv_close(handle, lcuB_onhandleclosed);
		}
	}
	if (req) setrequestop(op);
	else sethandleop(op);
	lcu_yieldop(L, narg, ctx, func, op);  /* never returns */
	return NULL;
}

LCULIB_API int lcuK_chkignoreop (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	int narg = (int)ctx;
	int ignore = lua_touserdata(L, -1) != loop;
	lcu_assert(status == LUA_YIELD);
	if (!lcu_isrequestop(op)) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (ignore && !uv_is_closing(handle)) uv_close(handle, lcuB_onhandleclosed);
	}
	setignoredop(op);  /* mark as not rescheduled */
	lua_pushboolean(L, !ignore);
	lua_insert(L, narg+1);
	return lua_gettop(L)-narg;
}
