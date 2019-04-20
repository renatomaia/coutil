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


static void savecoro (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushthread(L);
	lua_settable(L, LCU_COREGISTRY);
}

static void freecoro (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushnil(L);
	lua_settable(L, LCU_COREGISTRY);
}

LCULIB_API void lcu_freehdl (lua_State *L, uv_handle_t *handle) {
	handle->data = NULL;  /* mark as not pending */
	freecoro(L, (void *)handle);
}

LCULIB_API void lcu_freereq (lua_State *L, lcu_PendingOp *op) {
	lcu_assert(lcu_isrequestop(op));
	freecoro(L, (void *)op);
	op->flags = 0;
	lcu_tohandle(op)->type = UV_UNKNOWN_HANDLE;
}

LCULIB_API void lcu_chkinitiated (lua_State *L, void *key, int err) {
	lcu_chkerror(L, err);
	savecoro(L, key);
}

static void lcuB_onhandleclosed (uv_handle_t *handle) {
	handle->type = UV_UNKNOWN_HANDLE;
	lcu_PendingOp *op = (lcu_PendingOp *)handle;
	if (lcu_ispendingop(op))
		lcu_resumeop(op, handle->loop, (lua_State *)handle->data);
	else freecoro((lua_State *)handle->loop->data, (void *)handle);
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

LCULIB_API int lcu_resumecoro (lua_State *co, uv_loop_t *loop) {
	lua_State *L = (lua_State *)loop->data;
	lua_pushlightuserdata(co, loop);  /* token to sign scheduler resume */
	return lua_resume(co, L, lua_gettop(L));
}

LCULIB_API void lcu_resumeop (lcu_PendingOp *op, uv_loop_t *loop, lua_State *co) {
	int status;
	setignoredop(op);  /* coroutine not interested anymore */
	status = lcu_resumecoro(co, loop);
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
	if (lua_touserdata(L, -1) != loop) {  /* check token */
		lua_pushnil(L);
		lua_insert(L, 1);
		return 0;
	}
	lua_pop(L, 1);  /* discard token */
	return 1;
}

LCULIB_API void lcu_ignoreop (lcu_PendingOp *op, int scheduled) {
	if (!lcu_isrequestop(op)) {
		uv_handle_t *handle = lcu_tohandle(op);
		if (!scheduled && !uv_is_closing(handle))
			uv_close(handle, lcuB_onhandleclosed);
	}
	setignoredop(op);  /* mark as not rescheduled */
}

LCULIB_API int lcuK_chkignoreop (lua_State *L, int status, lua_KContext ctx) {
	int scheduled;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	scheduled = lcu_doresumed(L, lcu_toloop(L));
	lcu_ignoreop(lcu_getopof(L), scheduled);
	return lua_gettop(L);
}
