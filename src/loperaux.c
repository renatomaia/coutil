#include "lmodaux.h"
#include "loperaux.h"


#define COREGISTRY	lua_upvalueindex(2)
#define OPERATIONS	lua_upvalueindex(3)

#define FLAG_REQUEST  0x01
#define FLAG_PENDING  0x02

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
	return lua_resume(thread, L, 0);
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
	if (lua_gettable(L, LCU_OPERATIONS) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (Operation *)lua_newuserdata(L, sizeof(Operation));
		lua_settable(L, LCU_OPERATIONS);
		op->flags = FLAG_REQUEST;
		request = torequest(op);
		request->type = UV_UNKNOWN_REQ;
		request->data = (void *)L;
	}
	else op = (Operation *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return op;
}

static int endop (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = toloop(L);
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcuL_clearflag(op, FLAG_PENDING);
	if (haltedop(L, loop))
		if (!lcuL_hasflag(op, FLAG_REQUEST))
			cancelthrop(L, op);
	else
		if (op->results)
			return op->results(L);
	return lua_gettop(L)-narg; /* return yield */
}

static int startedop (lua_State *L, Operation *op, int err) {
	if (err >= 0) {
		int narg = lua_gettop(L);
		lcu_assert(!lcuL_hasflag(op, FLAG_PENDING));
		lcuL_setflag(op, FLAG_PENDING);
		return lua_yieldk(L, 0, (lua_KContext)narg, endop);
	} else if (!lcuL_hasflag(op, FLAG_REQUEST)) {
		lcu_assert(lcuL_hasflag(op, FLAG_PENDING));
		lcuL_clearflag(op, FLAG_PENDING);
		cancelthrop(op);
	}
	return lcuL_pusherror(L, err);
}

#define startop(L,O,S,L) startedop(L, O, (S)(L, tohandle(O), L));

static int resetop (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = toloop(L);
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(lcuL_hasflag(op, FLAG_PENDING));
	lcuL_clearflag(op, FLAG_PENDING);
	if (!haltedop(L, loop)) {
		int mkreq = lua_toboolean(L, narg-1);
		lcu_assert(lcuL_hasflag(op, FLAG_REQUEST));
		lcu_assert(torequest(op)->type == UV_UNKNOWN_REQ);
		if (mkreq) {
			lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startop(L, op, setup, loop);
		} else {
			lcu_HandleSetup setup = (lcu_HandleSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startop(L, op, setup, loop);
		}
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int yieldreset (lua_State *L, Operation *op, int mkreq, void *setup) {
	lcu_assert(!lcuL_hasflag(op, FLAG_PENDING));
	lua_pushboolean(L, mkreq);
	lua_pushlightuserdata(L, (void *)setup);
	lcuL_setflag(op, FLAG_PENDING);
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), resetop);
}

static int returnyield (lua_State *L) {
}

typedef enum { FREEOP, SAMEOP, WAITOP } opstatus;

static opstatus checkreset (Operation *op, lua_CFunction results, int type) {
	op->results = results;
	if (lcuL_hasflag(op, FLAG_REQUEST)) {
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

LCULIB_API int lcuT_resetreqopk (lua_State *L,
                                 lcu_RequestSetup setup,
                                 lua_CFunction results) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		switch (checkreset(op, results, 0)) {
			case FREEOP: return startop(L, op, setup, toloop(L));
			case WAITOP: return yieldreset(L, op, 1, (void *)setup);
		}
	}
	return luaL_error(L, "unable to yield");
}

LCULIB_API void lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)request;
	lcu_assert(lcuL_hasflag(op, FLAG_REQUEST));
	freethread(L, (void *)request);
	request->type = UV_UNKNOWN_REQ;
	if (lcuL_hasflag(op, FLAG_PENDING)) {
		lua_State *thread = (lua_State *)request->data;
		lcuL_pushresults(thread, 0, err);
		resumethread(thread, L, loop);
	}
}


/*
 * thread operation
 */

static void closedhdl (uv_handle_t *handle) {
	Operation *op = (Operation *)handle;
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	uv_req_t *request = torequest(op);
	lcu_assert(!lcuL_hasflag(op, FLAG_REQUEST));
	freethread(L, (void *)handle);
	lcuL_setflag(op, FLAG_REQUEST);
	request->type = UV_UNKNOWN_REQ;
	request->data = (void *)thread;
	if (lcuL_hasflag(op, FLAG_PENDING)) resumethread(thread, L, loop);
}

static void cancelthrop (Operation *op) {
	uv_handle_t *handle = tohandle(op);
	lcu_assert(!lcuL_hasflag(op, FLAG_REQUEST));
	if (!uv_is_closing(handle)) uv_close(handle, closedhdl);
}


LCULIB_API int lcuT_resetthropk (lua_State *L,
                                 uv_handle_type type,
                                 lcu_HandleSetup setup,
                                 lua_CFunction results) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		uv_loop_t *loop = NULL;
		lcu_assert(type);
		switch (checkreset(op, results, type)) {
			case FREEOP: loop = toloop(L);
			case SAMEOP: return startop(L, op, setup, loop);
			case WAITOP: return yieldreset(L, op, 0, (void *)setup);
		}
	}
	return luaL_error(L, "unable to yield");
}

LCULIB_API int lcuT_armthrop (lua_State *L, int err) {
	lcu_assert(lcuL_hasflag(tothrop(L), FLAG_REQUEST));
	lcu_assert(tohandle(tothrop(L))->type != UV_UNKNOWN_HANDLE);
	if (err >= 0) {
		Operation *op = tothrop(L);
		uv_handle_t *handle = tohandle(op);
		savethread(L, (void *)op);
		handle->data = (void *)L;
		lcuL_clearflag(op, FLAG_REQUEST);
		lcuL_setflag(op, FLAG_PENDING);
	}
	return err;
}

LCULIB_API int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)handle;
	int status;
	lcu_assert(!lcuL_hasflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_hasflag(op, FLAG_PENDING));
	status = resumethread(thread, L, loop)
	if (status != LUA_YIELD || !lcuL_hasflag(op, FLAG_PENDING)) cancelthrop(L, op);
	return status;
}


/*
 * object operation
 */

static void closedobj (uv_handle_t *handle) {
	lua_State *L = (lua_State *)handle->loop->data;
	freethread(L, (void *)handle);  /* becomes garbage */
}

LCULIB_API void lcu_closeobj (lua_State *L, int idx, uv_handle_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		lcu_assert(lua_gettop(thread) == 0);
		handle->data = NULL;
		lua_pushnil(thread);
		lua_pushliteral(thread, "closed");
		lua_resume(thread, L, 0);
	}
	lua_pushvalue(L, idx);  /* get the object */
	lua_pushlightuserdata(L, (void *)handle);
	lua_insert(L, -2);  /* place it below the object */
	lua_settable(L, COREGISTRY);  /* also 'freethread' */
	uv_close(handle, closedobj);
}

/*
LCULIB_API int lcu_close##OBJ (lua_State *L, int idx) {
	lcu_##OBJTYPE *object = to##OBJ(L, idx);
	if (object && lcu_isopen##OBJ(tcp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&object->handle);
		lcuL_setflag(object, LCU_OBJFLAG_CLOSED);
		return 1;
	}
	return 0;
}
*/

LCULIB_API int lcuT_awaitobj (lua_State *L, uv_handle_t *handle) {
	lcu_assert(handle->data == NULL);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_has_ref(handle));
	lcu_assert(!uv_is_closing(handle));
	savethread(L, (void *)handle);
	handle->data = (void *)L;
}

LCULIB_API void lcuT_releaseobj (lua_State *L, uv_handle_t *handle) {
	freethread(L, (void *)handle);
	handle->data = NULL;
}

LCULIB_API int lcuT_haltedobjop (lua_State *L, uv_handle_t *handle) {
	if (!haltedop(L, handle->loop)) return 0;
	lcuT_releaseobj(L, handle);
	return 1;
}

LCULIB_API int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	int status;
	handle->data = NULL;
	status = resumethread(thread, L, loop);
	if (status != LUA_YIELD || handle->data != thread) lcuT_releaseobj(L, handle);
	return status;
}
