#include "lmodaux.h"
#include "loperaux.h"


#define COREGISTRY	lua_upvalueindex(2)
#define OPERATIONS	lua_upvalueindex(3)

#define FLAG_REQUEST  0x01
#define FLAG_PENDING  0x02

#define hasflag(V,B) ((V)->flags&(B))
#define setflag(V,B) ((V)->flags |= (B))
#define clearflag(V,B) ((V)->flags &= ~(F))

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
	clearflag(op, FLAG_PENDING);
	if (haltedop(L, loop)) {
		if (!hasflag(op, FLAG_REQUEST)) cancelthrop(L, op);
		return lua_gettop(L)-narg; /* return yield */
	}
	return op->results(L);
}

static int startop (lua_State *L, Operation *op, int err) {
	if (err >= 0) {
		int narg = lua_gettop(L);
		lcu_assert(!hasflag(op, FLAG_PENDING));
		setflag(op, FLAG_PENDING);
		return lua_yieldk(L, 0, (lua_KContext)narg, endop);
	} else if (!hasflag(op, FLAG_REQUEST)) {
		lcu_assert(hasflag(op, FLAG_PENDING));
		clearflag(op, FLAG_PENDING);
		cancelthrop(op);
	}
	return lcuL_pusherror(L, err);
}

#define startreqop(L,O,S) startop(L, O, (S)(L, torequest(O)));

#define startthrop(L,O,S,I) startop(L, O, (S)(L, tohandle(O), I));

static int resetop (lua_State *L, int status, lua_KContext kctx) {
	uv_loop_t *loop = toloop(L);
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(hasflag(op, FLAG_PENDING));
	clearflag(op, FLAG_PENDING);
	if (!haltedop(L, loop)) {
		int mkreq = lua_toboolean(L, narg-1);
		lcu_assert(hasflag(op, FLAG_REQUEST));
		lcu_assert(torequest(op)->type == UV_UNKNOWN_REQ);
		if (mkreq) {
			lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startreqop(L, op, setup);
		} else {
			lcu_HandleSetup setup = (lcu_HandleSetup)lua_touserdata(L, narg);
			lua_settop(L, narg-2);  /* discard yield, 'setup' and 'mkreq' */
			return startthrop(L, op, setup, loop);
		}
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int yieldreset (lua_State *L, Operation *op, int mkreq, void *setup) {
	lcu_assert(!hasflag(op, FLAG_PENDING));
	lua_pushboolean(L, mkreq);
	lua_pushlightuserdata(L, (void *)setup);
	setflag(op, FLAG_PENDING);
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), resetop);
}

typedef enum { FREEOP, SAMEOP, WAITOP } opstatus;

static opstatus checkreset (Operation *op, int type) {
	if (hasflag(op, FLAG_REQUEST)) {
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
		op->results = results;
		switch (checkreset(op, 0)) {
			case FREEOP: return startreqop(L, op, setup);
			case WAITOP: return yieldreset(L, op, 1, (void *)setup);
		}
	}
	return luaL_error(L, "unable to yield");
}

LCULIB_API void lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)request;
	lcu_assert(hasflag(op, FLAG_REQUEST));
	freethread(L, (void *)request);
	request->type = UV_UNKNOWN_REQ;
	if (hasflag(op, FLAG_PENDING)) {
		lua_State *thread = (lua_State *)request->data;
		lcuL_doresults(thread, 0, err);
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
	lcu_assert(!hasflag(op, FLAG_REQUEST));
	freethread(L, (void *)handle);
	setflag(op, FLAG_REQUEST);
	request->type = UV_UNKNOWN_REQ;
	request->data = (void *)thread;
	if (hasflag(op, FLAG_PENDING)) resumethread(thread, L, loop);
}

static void cancelthrop (Operation *op) {
	uv_handle_t *handle = tohandle(op);
	lcu_assert(!hasflag(op, FLAG_REQUEST));
	if (!uv_is_closing(handle)) uv_close(handle, closedhdl);
}


LCULIB_API int lcuT_resetthropk (lua_State *L,
                                 uv_handle_type type,
                                 lcu_HandleSetup setup,
                                 lua_CFunction results) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		uv_loop_t *loop = NULL;
		op->results = results;
		lcu_assert(type);
		switch (checkreset(op, type)) {
			case FREEOP: loop = toloop(L);
			case SAMEOP: return startthrop(L, op, setup, loop);
			case WAITOP: return yieldreset(L, op, 0, (void *)setup);
		}
	}
	return luaL_error(L, "unable to yield");
}

LCULIB_API int lcuT_armthrop (lua_State *L, int err) {
	lcu_assert(hasflag(tothrop(L), FLAG_REQUEST));
	lcu_assert(tohandle(tothrop(L))->type != UV_UNKNOWN_HANDLE);
	if (err >= 0) {
		Operation *op = tothrop(L);
		uv_handle_t *handle = tohandle(op);
		savethread(L, (void *)op);
		handle->data = (void *)L;
		clearflag(op, FLAG_REQUEST);
		setflag(op, FLAG_PENDING);
	}
	return err;
}

LCULIB_API int lcuU_resumethrop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)handle;
	int status;
	lcu_assert(!hasflag(op, FLAG_REQUEST));
	lcu_assert(hasflag(op, FLAG_PENDING));
	status = resumethread(thread, L, loop)
	if (status != LUA_YIELD || !hasflag(op, FLAG_PENDING))
		cancelthrop(L, op);
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
		setflag(object, LCU_OBJFLAG_CLOSED);
		return 1;
	}
	return 0;
}
*/

static void releaseobj (lua_State *L, uv_handle_t *handle) {
	freethread(L, (void *)handle);
	handle->data = NULL;
}

LCULIB_API int lcuU_resumeobjop (lua_State *thread, uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	int status;
	handle->data = NULL;
	status = resumethread(thread, L, loop);
	if (status != LUA_YIELD || handle->data != thread) releaseobj(L, handle);
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
	if (!lua_isyieldable(L)) return luaL_error(L, "unable to yield");
	savethread(L, (void *)handle);
	handle->data = (void *)L;
	return lua_yieldk(L, 0, kctx, kfn);
}
