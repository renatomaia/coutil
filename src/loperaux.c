#include "lmodaux.h"
#include "loperaux.h"


#define LCU_COREGISTRY	lua_upvalueindex(2)
#define LCU_OPERATIONS	lua_upvalueindex(3)


LCULIB_API lua_State *lcu_getopcoro(lcu_PendingOp *op) {
	void *data = NULL;
	switch (lcu_getoptype(op)) {
		case LCU_OPTYPE_GLOBAL: data = lcu_toglobalop(op)->data; break;
		case LCU_OPTYPE_OBJECT: data = lcu_toobjectop(op)->data; break;
		case LCU_OPTYPE_REQUEST: data = lcu_torequestop(op)->data; break;
		default: lcu_assert(0);
	}
	return (lua_State *)data;
}

LCULIB_API uv_loop_t *lcu_getoploop(lcu_PendingOp *op) {
	switch (lcu_getoptype(op)) {
		case LCU_OPTYPE_GLOBAL: return lcu_toglobalop(op)->loop;
		case LCU_OPTYPE_OBJECT: return lcu_toobjectop(op)->loop;
		case LCU_OPTYPE_REQUEST: return (op)->kind.request.loop;
	}
	lcu_assert(0);
	return NULL;
}

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
	lua_pushlightuserdata(L, handle);
	lua_pushnil(L);
	lua_settable(L, LCU_COREGISTRY);
}

LCULIB_API void lcu_chkinitop (lua_State *L,
                               lcu_PendingOp *op,
                               uv_loop_t * loop,
                               int err) {
	lcu_chkerror(L, err);
	saveopcoro(op, L);
	if (lcu_getoptype(op) == LCU_OPTYPE_REQUEST) op->kind.request.loop = loop;
}

LCULIB_API void lcu_freeop (lcu_PendingOp *op) {
	freeopcoro(op);
	op->flags = 0;
	lcu_toglobalop(op)->type = UV_UNKNOWN_HANDLE;
}

static void lcuB_onhandleclosed (uv_handle_t *handle) {
	handle->type = UV_UNKNOWN_HANDLE;
	lua_State *co = (lua_State *)handle->data;
	lcu_PendingOp *op = (lcu_PendingOp *)handle;  // TODO: not valid for object ops!
	lcu_assert(lcu_getoptype(op) == LCU_OPTYPE_GLOBAL);
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


LCULIB_API int lcu_yieldhdl (lua_State *L,
                             lua_KContext ctx,
                             lua_KFunction func,
                             uv_handle_t *handle) {
	handle->data = (void *)L;
	return lua_yieldk(L, 0, ctx, func);
}

LCULIB_API int lcu_yieldop (lua_State *L,
                            lua_KContext ctx,
                            lua_KFunction func,
                            lcu_PendingOp *op) {
	setpendingop(op);
	if (!lcu_isrequestop(op)) return lcu_yieldhdl(L, ctx, func, lcu_tohandle(op));
	lcu_torequest(op)->data = (void *)L;
	return lua_yieldk(L, 0, ctx, func);
}

LCULIB_API void lcu_resumecoro (lua_State *co, uv_loop_t *loop) {
	lua_State *L = (lua_State *)loop->data;
	lua_pushlightuserdata(co, loop);  /* token to sign scheduler resume */
	return lua_resume(co, L, lua_gettop(L));
}

LCULIB_API void lcu_resumeop (lcu_PendingOp *op, lua_State *co) {
	uv_loop_t *loop = lcu_getoploop(op);
	int status;
	setignoredop(op);  /* coroutine not interested anymore */
	lcu_resumecoro(co, loop);
	if (!lcu_isrequestop(op) && (status != LUA_YIELD || !lcu_ispendingop(op)) ) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (handle->type != UV_UNKNOWN_HANDLE && !uv_is_closing(handle))
			uv_close(handle, lcuB_onhandleclosed);
	}
}

LCULIB_API lcu_PendingOp *lcu_resetop (lua_State *L, int req, int type,
                                       lua_KContext ctx, lua_KFunction func) {
	lcu_PendingOp *op = lcu_getopof(L);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (!lcu_isrequestop(op)) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (handle->type == UV_UNKNOWN_HANDLE) {  /* unsued operation */
			if (req) setrequestop(op);
			lua_pushlightuserdata(L, lcu_toloop(L));  /* token to sign scheduled */
			func(L, LUA_YIELD, ctx);  /* never returns */
		} else if (!uv_is_closing(handle)) {
			if (!req && handle->type == type) return op;
			uv_close(handle, lcuB_onhandleclosed);
		}
	}
	if (req) setrequestop(op);
	else sethandleop(op);
	lcu_yieldop(L, ctx, func, op);  /* never returns */
	return NULL;
}

LCULIB_API int lcu_doresumed (lua_State *L, uv_loop_t *loop) {
	int interrupt = ;
	if (lua_touserdata(L, -1) != loop) {  /* check token */
		lua_pushnil(L);
		lua_insert(L, 1);
		return 0;
	}
	lua_pop(L, 1);  /* discard token */
	return 1;
}

LCULIB_API void lcu_ignoreop (lcu_PendingOp *op) {
	if (!lcu_isrequestop(op)) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (interrupt && !uv_is_closing(handle))
			uv_close(handle, lcuB_onhandleclosed);
	}
	setignoredop(op);  /* mark as not rescheduled */
}

LCULIB_API int lcuK_chkignoreop (lua_State *L, int status, lua_KContext ctx) {
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	lcu_ignoreop(lcu_getopof(L))
	lcu_doresumed(L, lcu_toloop(L));
	return lua_gettop(L);
}
