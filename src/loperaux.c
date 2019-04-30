#include "lmodaux.h"
#include "loperaux.h"


#define LCU_COREGISTRY	lua_upvalueindex(2)
#define LCU_OPERATIONS	lua_upvalueindex(3)


static void savethread (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushthread(L);
	lua_settable(L, LCU_COREGISTRY);
}

static void freethread (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushnil(L);
	lua_settable(L, LCU_COREGISTRY);
}

static int resumethread (lua_State *thread, lua_State *L, uv_loop_t *loop) {
	lcu_assert(loop->data == (void *)L);
	lua_pushlightuserdata(thread, loop);  /* token to sign scheduler resume */
	return lua_resume(thread, L, 0);
}

LCULIB_API void lcu_freethrop (lcu_Operation *op) {
	uv_handle_t *handle = lcu_tohandle(op);
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_PENDING));
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_REQUEST));
	if (!uv_is_closing(handle)) uv_close(handle, lcuB_closedhdl);
}

LCULIB_API int lcuU_endthrop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lcu_Operation *op = (lcu_Operation *)handle;
	int status;
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_REQUEST));
	lcu_assert(lcu_testflag(op, LCU_OPFLAG_PENDING));
	lcu_clearflag(op, LCU_OPFLAG_PENDING);
	status = resumethread(thread, L, loop)
	if (status != LUA_YIELD || !lcu_testflag(op, LCU_OPFLAG_PENDING))
		lcu_freethrop(L, op);
	return status;
}

LCULIB_API int lcuU_endreqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *L = (lua_State *)loop->data;
	lcu_Operation *op = (lcu_Operation *)request;
	freethread(L, (void *)request);
	request->type = UV_UNKNOWN_REQ;
	if (lcu_testflag(op, LCU_OPFLAG_PENDING)) {
		lua_State *thread = (lua_State *)request->data;
		lcu_clearflag(op, LCU_OPFLAG_PENDING);
		lcuL_doresults(thread, 0, err);
		resumethread(thread, L, loop);
	}
}

static lcu_Operation *getthreadop (lua_State *L) {
	lcu_Operation *op;
	lua_pushthread(L);
	if (lua_gettable(L, LCU_OPERATIONS) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (lcu_Operation *)lua_newuserdata(L, sizeof(lcu_Operation));
		lua_settable(L, LCU_OPERATIONS);
		op->flags = LCU_OPFLAG_REQUEST;
		request = lcu_torequest(op);
		request->type = UV_UNKNOWN_REQ;
		request->data = (void *)L;
	}
	else op = (lcu_Operation *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return op;
}

LCULIB_API int lcuT_doneop (lua_State *L, uv_loop_t *loop) {
	if (lua_touserdata(L, -1) == loop) {
		lua_pop(L, 1);  /* discard token */
		return 1;
	}
	return 0;
}

LCULIB_API int lcuT_donethrop (lua_State *L,
                               uv_loop_t *loop,
                               lcu_Operation *op) {
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_REQUEST));
	if (!lcu_doresumed(L, loop)) {
		lcu_clearflag(op, LCU_OPFLAG_PENDING);
		lcu_freethrop(L, op);
		return 1;
	}
	return 0;
}

LCULIB_API lcu_Operation *lcuT_resetopk (lua_State *L,
                                         int request,
                                         int type,
                                         lua_KContext kctx,
                                         lua_KFunction setup) {
	lcu_Operation *op = getthreadop(L);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) {
		uv_req_t *request = lcu_torequest(op);
		if (request->type == UV_UNKNOWN_REQ) {  /* unsued operation */
			lua_pushlightuserdata(L, lcu_toloop(L));  /* token to sign scheduled */
			setup(L, LUA_YIELD, kctx);  /* never returns */
		}
	} else {
		uv_handle_t *handle = lcu_tohandle(op);
		if (!uv_is_closing(handle)) {
			if (!request && handle->type == type) return op;
			uv_close(handle, lcuB_closedhdl);
		}
	}
	lcu_setflag(op, LCU_OPFLAG_PENDING);
	return lua_yieldk(L, 0, kctx, setup);
}

LCULIB_API void lcuT_armthrop (lua_State *L, lcu_Operation *op) {
	lcu_assert(lcu_testflag(op, LCU_OPFLAG_REQUEST));
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_PENDING));
	lcu_assert(lcu_torequest(op)->data == (void *)L);
	savethread(L, (void *)op);
	lcu_clearflag(op, LCU_OPFLAG_REQUEST);
	lcu_tohandle(op)->data = (void *)L;
}

static void lcuB_closedhdl (uv_handle_t *handle) {
	lcu_Operation *op = (lcu_Operation *)handle;
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	uv_req_t *request = lcu_torequest(op);
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_REQUEST));
	freethread(L, (void *)handle);
	lcu_setflag(op, LCU_OPFLAG_REQUEST);
	request->type = UV_UNKNOWN_REQ;
	request->data = (void *)thread;
	if (lcu_testflag(op, LCU_OPFLAG_PENDING)) {
		lcu_clearflag(op, LCU_OPFLAG_PENDING);
		lcu_resumeop(op, loop, thread);
	}
}

LCULIB_API int lcuT_awaitopk (lua_State *L,
                              lcu_Operation *op,
                              lua_KContext kctx,
                              lua_KFunction kfn) {
	if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) {
		uv_req_t *request = lcu_torequest(op)
		lcu_assert(request->data == (void *)L);
		lcu_assert(request->type != UV_UNKNOWN_REQ);
	} else {
		uv_handle_t *handle = lcu_tohandle(op)
		lcu_assert(handle->data == (void *)L);
		lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
		lcu_assert(uv_has_ref(handle));
		lcu_assert(!uv_is_closing(handle));
	}
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) savethread(L, (void *)op);
	lcu_setflag(op, LCU_OPFLAG_PENDING);
	return lua_yieldk(L, 0, kctx, kfn);
}


static void lcuB_closedobj (uv_handle_t *handle) {
	lua_State *L = (lua_State *)handle->loop->data;
	freethread(L, (void *)handle);  /* becomes garbage */
}

LCULIB_API void lcu_closeobj (lua_State *L, int idx, uv_handle_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		lcu_assert(lua_gettop(thread) == 0);
		lua_pushnil(thread);
		lua_pushliteral(thread, "closed");
		lua_resume(thread, L, 0);
	}
	lua_pushvalue(L, idx);  /* get the object */
	lua_pushlightuserdata(L, (void *)handle);
	lua_insert(L, -2);  /* place it below the object */
	lua_settable(L, LCU_COREGISTRY);
	handle->data = NULL;
	uv_close(handle, lcuB_closedobj);
}

/*
LCULIB_API int lcu_close##OBJ (lua_State *L, int idx) {
	lcu_##OBJTYPE *object = lcu_to##OBJ(L, idx);
	if (object && lcu_isopen##OBJ(tcp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&object->handle);
		lcu_setflag(object, LCU_OBJFLAG_CLOSED);
		return 1;
	}
	return 0;
}
*/

LCULIB_API void lcu_releaseobj (lua_State *L, uv_handle_t *handle) {
	freethread(L, (void *)handle);
	handle->data = NULL;
}

LCULIB_API int lcuU_endobjop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	int status;
	handle->data = NULL;
	status = resumethread(thread, L, loop);
	if (status != LUA_YIELD || handle->data != thread) lcu_releaseobj(L, handle);
	return status;
}

LCULIB_API int lcuT_awaitobjk (lua_State *L,
                               uv_handle_t *handle,
                               lua_KContext kctx,
                               lua_KFunction kfn) {
	lcu_assert(handle->data == NULL);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_has_ref(handle));
	lcu_assert(!uv_is_closing(handle));
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	savethread(L, (void *)handle);
	handle->data = (void *)L;
	return lua_yieldk(L, 0, kctx, kfn);
}
