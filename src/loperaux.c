#include "lmodaux.h"
#include "loperaux.h"


#define COREGISTRY	lua_upvalueindex(1)
#define OPERATIONS	lua_upvalueindex(2)

#define FLAG_REQUEST  0x01
#define FLAG_PENDING  0x02
#define FLAG_OBJOPON  0x04

#define torequest(O) ((uv_req_t *)&((O)->kind.request))
#define tohandle(O) ((uv_handle_t *)&((O)->kind.handle))

static void savethread (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushthread(L);
	lua_settable(L, COREGISTRY);
}

static void freethread (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushnil(L);
	lua_settable(L, COREGISTRY);
}

static int resumethread (lua_State *thread, lua_State *L, uv_loop_t *loop) {
	lcu_assert(loop->data == (void *)L);
	lua_pushlightuserdata(thread, loop);  /* token to sign scheduler resume */
	return lua_resume(thread, L, lua_gettop(thread));
}

static int haltedop (lua_State *L, uv_loop_t *loop) {
	if (lua_touserdata(L, -1) == loop) {
		lua_pop(L, 1);  /* discard token */
		return 0;
	}
	return 1;
}

typedef struct Operation {
	union {
		union uv_any_req request;
		union uv_any_handle handle;
	} kind;
	int flags;
	lua_CFunction results;
} Operation;

static Operation *tothrop (lua_State *L) {
	Operation *op;
	lua_pushthread(L);
	if (lua_gettable(L, OPERATIONS) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (Operation *)lua_newuserdata(L, sizeof(Operation));
		lua_settable(L, OPERATIONS);
		op->flags = FLAG_REQUEST;
		request = torequest(op);
		request->type = UV_UNKNOWN_REQ;
		request->data = (void *)L;
	}
	else op = (Operation *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return op;
}

static void closedhdl (uv_handle_t *handle) {
	Operation *op = (Operation *)handle;
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	void *thread = handle->data;
	uv_req_t *request = torequest(op);
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	freethread(L, (void *)handle);
	lcuL_setflag(op, FLAG_REQUEST);
	request->type = UV_UNKNOWN_REQ;
	request->data = thread;
	if (lcuL_maskflag(op, FLAG_PENDING))
		resumethread((lua_State *)thread, L, loop);
}

static void cancelop (Operation *op) {
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		uv_cancel(request);
	} else {
		uv_handle_t *handle = tohandle(op);
		if (!uv_is_closing(handle)) uv_close(handle, closedhdl);
	}
}

static int endop (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = lcu_toloop(L);
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcuL_clearflag(op, FLAG_PENDING);
	if (haltedop(L, loop)) cancelop(op);
	else if (op->results) return op->results(L);
	return lua_gettop(L)-narg; /* return yield */
}

static int startedop (lua_State *L, Operation *op, int err) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING));
	if (err >= 0) {
		lcuL_setflag(op, FLAG_PENDING);
		if (lcuL_maskflag(op, FLAG_REQUEST)) savethread(L, (void *)op);
		return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), endop);
	}
	else cancelop(op);
	return lcuL_pushresults(L, 0, err);
}

#define startreqop(L,O,S,U) startedop(L, O, (S)(L, torequest(O), U))
#define startthrop(L,O,S,U) startedop(L, O, (S)(L, tohandle(O), U))

static int resetop (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = lcu_toloop(L);
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	lcuL_clearflag(op, FLAG_PENDING);
	if (!haltedop(L, loop)) {
		int mkreq = lua_toboolean(L, narg-1);
		lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
		lcu_assert(torequest(op)->type == UV_UNKNOWN_REQ);
		if (mkreq) {
			lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startreqop(L, op, setup, loop);
		} else {
			lcu_HandleSetup setup = (lcu_HandleSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startthrop(L, op, setup, loop);
		}
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int yieldreset (lua_State *L, Operation *op, int mkreq, void *setup) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING));
	lua_pushboolean(L, mkreq);
	lua_pushlightuserdata(L, (void *)setup);
	lcuL_setflag(op, FLAG_PENDING);
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), resetop);
}

typedef enum { FREEOP, SAMEOP, WAITOP } opstatus;

static opstatus checkreset (Operation *op, lua_CFunction results, int type) {
	op->results = results;
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		if (request->type == UV_UNKNOWN_REQ) return FREEOP;
	} else {
		uv_handle_t *handle = tohandle(op);
		if (!uv_is_closing(handle)) {
			if (type && handle->type == type) return SAMEOP;
			uv_close(handle, closedhdl);
		}
	}
	return WAITOP;
}


/*
 * request operation
 */

LCUI_FUNC int lcuT_resetreqopk (lua_State *L,
                                lcu_RequestSetup setup,
                                lua_CFunction results) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		switch (checkreset(op, results, 0)) {
			case FREEOP: return startreqop(L, op, setup, lcu_toloop(L));
			case WAITOP: return yieldreset(L, op, 1, (void *)setup);
			default: return 0;
		}
	}
	return luaL_error(L, "unable to yield");
}

LCUI_FUNC lua_State *lcuU_endreqop (uv_loop_t *loop, uv_req_t *request) {
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)request;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	request->type = UV_UNKNOWN_REQ;
	if (lcuL_maskflag(op, FLAG_PENDING)) return (lua_State *)request->data;
	freethread(L, (void *)request);
	return NULL;
}

LCUI_FUNC void lcuU_resumereqop (lua_State *thread,
                                 uv_loop_t *loop,
                                 uv_req_t *request) {
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)request;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	resumethread(thread, L, loop);
	if (lcuL_maskflag(op, FLAG_REQUEST) && request->type == UV_UNKNOWN_REQ)
		freethread(L, (void *)request);
}

LCUI_FUNC void lcuU_completereqop (uv_loop_t *loop,
                                   uv_req_t *request,
                                   int err) {
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		lcu_assert(lua_gettop(thread) == 0);
		lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, loop, request);
	}
}


/*
 * thread operation
 */

LCUI_FUNC int lcuT_resetthropk (lua_State *L,
                                uv_handle_type type,
                                lcu_HandleSetup setup,
                                lua_CFunction results) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		uv_loop_t *loop = NULL;
		lcu_assert(type);
		switch (checkreset(op, results, type)) {
			case FREEOP: loop = lcu_toloop(L);
			case SAMEOP: return startthrop(L, op, setup, loop);
			case WAITOP: return yieldreset(L, op, 0, (void *)setup);
		}
	}
	return luaL_error(L, "unable to yield");
}

LCUI_FUNC int lcuT_armthrop (lua_State *L, int err) {
	lcu_assert(lcuL_maskflag(tothrop(L), FLAG_REQUEST));
	lcu_assert(tohandle(tothrop(L))->type != UV_UNKNOWN_HANDLE);
	if (err >= 0) {
		Operation *op = tothrop(L);
		uv_handle_t *handle = tohandle(op);
		savethread(L, (void *)op);
		handle->data = (void *)L;
		lcuL_clearflag(op, FLAG_REQUEST);
		//lcuL_setflag(op, FLAG_PENDING);
	}
	return err;
}

LCUI_FUNC int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)handle;
	int status;
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	status = resumethread(thread, L, loop);
	if (status != LUA_YIELD || !lcuL_maskflag(op, FLAG_PENDING)) cancelop(op);
	return status;
}


/*
 * object operation
 */

static void closedobj (uv_handle_t *handle) {
	lua_State *L = (lua_State *)handle->loop->data;
	freethread(L, (void *)handle);  /* becomes garbage */
}

LCUI_FUNC void lcu_closeobjhdl (lua_State *L, int idx, uv_handle_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		lcu_assert(lua_gettop(thread) == 0);
		handle->data = NULL;
		lua_pushnil(thread);
		lua_pushliteral(thread, "closed");
		lua_resume(thread, L, 2);
	}
	lua_pushvalue(L, idx);  /* get the object */
	lua_pushlightuserdata(L, (void *)handle);
	lua_insert(L, -2);  /* place it below the object */
	lua_settable(L, COREGISTRY);  /* also 'freethread' */
	uv_close(handle, closedobj);
}

LCUI_FUNC void lcuT_awaitobj (lua_State *L, uv_handle_t *handle) {
	lcu_assert(handle->data == NULL);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_has_ref(handle));
	lcu_assert(!uv_is_closing(handle));
	savethread(L, (void *)handle);
	handle->data = (void *)L;
}

LCUI_FUNC int lcuT_haltedobjop (lua_State *L, uv_handle_t *handle) {
	if (!haltedop(L, handle->loop)) return 0;
	freethread(L, (void *)handle);
	handle->data = NULL;
	return 1;
}

LCUI_FUNC int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	int status;
	handle->data = NULL;
	status = resumethread(thread, L, loop);
	if (handle->data == NULL) freethread(L, (void *)handle);
	return status;
}
