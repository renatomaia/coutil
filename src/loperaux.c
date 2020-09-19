#include "lmodaux.h"
#include "loperaux.h"


#define OPERATIONS	lua_upvalueindex(1)

#define FLAG_REQUEST  0x01
#define FLAG_PENDING  0x02
#define FLAG_CANCEL  0x04

#define torequest(O) ((uv_req_t *)&((O)->kind.request))
#define tohandle(O) ((uv_handle_t *)&((O)->kind.handle))

LCUI_FUNC void lcuT_savevalue (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_insert(L, -2);
	lua_settable(L, LUA_REGISTRYINDEX);
}

LCUI_FUNC void lcuT_freevalue (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushnil(L);
	lua_settable(L, LUA_REGISTRYINDEX);
}

static void savethread (lua_State *L, void *key) {
	lua_pushthread(L);
	lcuT_savevalue(L, key);
}

static int resumethread (lua_State *thread,
                         lua_State *L,
                         int narg,
                         uv_loop_t *loop) {
	int nret, status;
	lcu_assert(loop->data == (void *)L);
	lua_pushlightuserdata(thread, loop);  /* token to sign scheduler resume */
	status = lua_resume(thread, L, narg+1, &nret);
	lua_pop(thread, nret);  /* dicard yielded values */
	return status;
}

static int haltedop (lua_State *L, void *token) {
	if (lua_touserdata(L, -1) == token) {
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
	lua_CFunction cancel;
} Operation;

static Operation *tothrop (lua_State *L) {
	Operation *op;
	lua_pushthread(L);
	if (lua_gettable(L, OPERATIONS) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (Operation *)lua_newuserdatauv(L, sizeof(Operation), 0);
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
	lua_State *thread = (lua_State *)handle->data;
	uv_req_t *request = torequest(op);
	lcu_Scheduler *sched = lcu_tosched(loop);
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	lcuT_freevalue(L, (void *)handle);
	if (handle->type == UV_ASYNC) sched->nasync--;
	else sched->nactive--;
	lcuL_setflag(op, FLAG_REQUEST);
	request->type = UV_UNKNOWN_REQ;
	request->data = thread;
	if (lcuL_maskflag(op, FLAG_PENDING))
		resumethread(thread, L, 0, loop);
}

static void cancelop (Operation *op) {
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		uv_cancel(request);
	} else {
		uv_handle_t *handle = tohandle(op);
		if (!lcuL_maskflag(op, FLAG_CANCEL) && !uv_is_closing(handle))
			uv_close(handle, closedhdl);
	}
}

static int endop (lua_State *L, int status, lua_KContext kctx) {
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg);
	lua_remove(L, narg--);
	lcuL_clearflag(op, FLAG_PENDING);
	if (haltedop(L, sched)) {
		if (op->cancel == NULL || op->cancel(L)) cancelop(op);
		else lcuL_setflag(op, FLAG_CANCEL);
	} else if (op->results) {
		return op->results(L);
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int startedop (lua_State *L,
                      lcu_Scheduler *sched,
                      Operation *op,
                      int nret) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING));
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		if (request->type != UV_UNKNOWN_REQ) {
			sched->nactive++;
		}
	}
	if (nret < 0) {  /* shall yield, and wait for callback */
		lcuL_setflag(op, FLAG_PENDING);
		if (lcuL_maskflag(op, FLAG_REQUEST)) savethread(L, (void *)op);
		lua_pushlightuserdata(L, (void *)sched);
		return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), endop);
	}
	else cancelop(op);
	return nret;
}

#define startreqop(L,S,O,F) startedop(L, S, O, (F)(L, torequest(O), lcu_toloop(S)))
#define startthrop(L,S,O,F,U) startedop(L, S, O, (F)(L, tohandle(O), U))

static int resetop (lua_State *L, int status, lua_KContext kctx) {
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg-2);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	lcuL_clearflag(op, FLAG_PENDING);
	if (!haltedop(L, sched)) {
		int mkreq = lua_toboolean(L, narg);
		lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
		lcu_assert(torequest(op)->type == UV_UNKNOWN_REQ);
		if (mkreq) {
			lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg-1);
			lua_settop(L, narg-3);  /* discard yield, 'sched', 'setup' and 'mkreq' */
			return startreqop(L, sched, op, setup);
		} else {
			lcu_HandleSetup setup = (lcu_HandleSetup)lua_touserdata(L, narg-1);
			lua_settop(L, narg-3);  /* discard yield, 'sched', 'setup' and 'mkreq' */
			return startthrop(L, sched, op, setup, lcu_toloop(sched));
		}
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int yieldreset (lua_State *L,
                       lcu_Scheduler *sched,
                       Operation *op,
                       int mkreq,
                       void *setup) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING));
	lua_pushlightuserdata(L, (void *)sched);
	lua_pushlightuserdata(L, (void *)setup);
	lua_pushboolean(L, mkreq);
	lcuL_setflag(op, FLAG_PENDING);
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), resetop);
}

typedef enum { FREEOP, SAMEOP, WAITOP } OpStatus;

static OpStatus checkreset (Operation *op,
                            lua_CFunction results,
                            lua_CFunction cancel,
                            int type) {
	op->results = results;
	op->cancel = cancel;
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		if (request->type == UV_UNKNOWN_REQ) return FREEOP;
	} else {
		uv_handle_t *handle = tohandle(op);
		if (!lcuL_maskflag(op, FLAG_CANCEL) && !uv_is_closing(handle)) {
			if (type && handle->type == type) return SAMEOP;
			uv_close(handle, closedhdl);
		}
	}
	return WAITOP;
}


LCUI_FUNC void lcuU_checksuspend(uv_loop_t *loop) {
	lua_State *L = (lua_State *)loop->data;
	lcu_Scheduler *sched = lcu_tosched(loop);
	if (sched->nactive == 0 && sched->nasync > 0) {
		lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY);
		uv_stop(loop);
	}
}


/*
 * request operation
 */

LCUI_FUNC int lcuT_resetreqopk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		switch (checkreset(op, results, cancel, 0)) {
			case FREEOP: return startreqop(L, sched, op, setup);
			case WAITOP: return yieldreset(L, sched, op, 1, (void *)setup);
			default: return 0;
		}
	}
	return luaL_error(L, "unable to yield");
}

LCUI_FUNC lua_State *lcuU_endreqop (uv_loop_t *loop, uv_req_t *request) {
	lua_State *L = (lua_State *)loop->data;
	Operation *op = (Operation *)request;
	lcu_Scheduler *sched = lcu_tosched(loop);
	sched->nactive--;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	request->type = UV_UNKNOWN_REQ;
	if (lcuL_maskflag(op, FLAG_PENDING)) return (lua_State *)request->data;
	lcuL_clearflag(op, FLAG_CANCEL);
	lcuT_freevalue(L, (void *)request);
	return NULL;
}

LCUI_FUNC int lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int narg) {
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)request->data;
	Operation *op = (Operation *)request;
	int status;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	status = resumethread(thread, L, narg, loop);
	if (lcuL_maskflag(op, FLAG_REQUEST) && request->type == UV_UNKNOWN_REQ)
		lcuT_freevalue(L, (void *)request);
	return status;
}


/*
 * thread operation
 */

LCUI_FUNC int lcuT_resetthropk (lua_State *L,
                                uv_handle_type type,
                                lcu_Scheduler *sched,
                                lcu_HandleSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel) {
	if (lua_isyieldable(L)) {
		Operation *op = tothrop(L);
		uv_loop_t *loop = NULL;
		lcu_assert(type);
		switch (checkreset(op, results, cancel, type)) {
			case FREEOP: loop = lcu_toloop(sched);
			case SAMEOP: return startthrop(L, sched, op, setup, loop);
			case WAITOP: return yieldreset(L, sched, op, 0, (void *)setup);
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
		lcu_Scheduler *sched = lcu_tosched(handle->loop);
		savethread(L, (void *)op);
		handle->data = (void *)L;
		lcuL_clearflag(op, FLAG_REQUEST);
		if (handle->type == UV_ASYNC) sched->nasync++;
		else sched->nactive++;
	}
	return err;
}

LCUI_FUNC int lcuU_endthrop (uv_handle_t *handle) {
	Operation *op = (Operation *)handle;
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	if (lcuL_maskflag(op, FLAG_PENDING)) return 1;
	lcuL_clearflag(op, FLAG_CANCEL);
	cancelop(op);
	return 0;
}

LCUI_FUNC int lcuU_resumethrop (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	Operation *op = (Operation *)handle;
	int status;
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	status = resumethread(thread, L, narg, loop);
	if (status != LUA_YIELD || !lcuL_maskflag(op, FLAG_PENDING)) cancelop(op);
	lcuU_checksuspend(loop);
	return status;
}

/*
 * object operation
 */

static void closedobj (uv_handle_t *handle) {
	lua_State *L = (lua_State *)handle->loop->data;
	lcuT_freevalue(L, (void *)handle);  /* becomes garbage */
}

LCUI_FUNC void lcuT_closeobjhdl (lua_State *L, int idx, uv_handle_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		int nret;
		handle->data = NULL;
		lua_pushnil(thread);
		lua_pushliteral(thread, "closed");
		lua_resume(thread, L, 2, &nret);
		lua_pop(thread, nret);  /* dicard yielded values */
	}
	lua_pushvalue(L, idx);  /* get the object */
	lua_pushlightuserdata(L, (void *)handle);
	lua_insert(L, -2);  /* place it below the object */
	lua_settable(L, LUA_REGISTRYINDEX);  /* also 'lcuT_freevalue' */
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
	lcu_assert(handle->data != NULL);
	if (!haltedop(L, handle->loop)) return 0;
	lcuT_freevalue(L, (void *)handle);
	handle->data = NULL;
	return 1;
}

LCUI_FUNC int lcuU_resumeobjop (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	int status = resumethread(thread, L, narg, loop);
	if (handle->data == NULL) lcuT_freevalue(L, (void *)handle);
	return status;
}
