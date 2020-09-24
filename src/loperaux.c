#include "lmodaux.h"
#include "loperaux.h"


#define FLAG_REQUEST  0x01
#define FLAG_PENDING  0x02
#define FLAG_CANCEL  0x04

#define torequest(O) ((uv_req_t *)&((O)->kind.request))
#define tohandle(O) ((uv_handle_t *)&((O)->kind.handle))


struct lcu_Scheduler {
	uv_loop_t loop;
	int nasync;  /* number of active 'uv_async_t' handles */
	int nactive;  /* number of other active handles */
};


static void pushhandlemap (lua_State *L) {
	lua_pushlightuserdata(L, pushhandlemap);
	if (lua_gettable(L, LUA_REGISTRYINDEX) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushlightuserdata(L, pushhandlemap);
		lua_pushvalue(L, -2);
		lua_createtable(L, 0, 1);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
}

static void closehandle (uv_handle_t* handle, void* arg) {
	if (!uv_is_closing(handle)) uv_close(handle, NULL);
}

static int terminateloop (lua_State *L) {
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, 1);
	uv_loop_t *loop = lcu_toloop(sched);
	int err = uv_loop_close(loop);
	if (err == UV_EBUSY) {
		uv_walk(loop, closehandle, NULL);
		loop->data = (void *)L;
		err = uv_run(loop, UV_RUN_DEFAULT);
		loop->data = NULL;
		if (!err) err = uv_loop_close(loop);
		else {
			lcuL_warnerr(L, "system.run: ", err);
			uv_print_all_handles(loop, stderr);
		}
	}
	lcu_assert(!err);
	return 0;
}

LCUI_FUNC void lcuM_newmodupvs (lua_State *L) {
	int err;
	lcu_Scheduler *sched;
	uv_loop_t *loop;
	pushhandlemap(L);  /* to be collected after the 'lcu_Scheduler' */
	lua_pop(L, 1);
	sched = (lcu_Scheduler *)lua_newuserdatauv(L, sizeof(lcu_Scheduler), 0);
	sched->nasync = 0;
	sched->nactive = 0;
	loop = lcu_toloop(sched);
	err = uv_loop_init(loop);
	if (err < 0) lcu_error(L, err);
	lcuL_setfinalizer(L, terminateloop);
	loop->data = NULL;
}

LCUI_FUNC void lcuT_savevalue (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_insert(L, -2);
	lua_settable(L, LUA_REGISTRYINDEX);
}

LCUI_FUNC void lcuT_pushsaved (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_gettable(L, LUA_REGISTRYINDEX);
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

static void resumethread (lua_State *thread,
                          lua_State *L,
                          int narg,
                          uv_loop_t *loop) {
	int nret, status;
	lcu_assert(loop->data == (void *)L);
	lua_pushlightuserdata(thread, loop);  /* token to sign scheduler resume */
	status = lua_resume(thread, L, narg+1, &nret);
	if (status != LUA_OK && status != LUA_YIELD) {
		const char *errmsg = lua_tostring(thread, -1);
		if (errmsg == NULL) errmsg = "(error object is not a string)";
		lcuL_warnmsg(L, "system.run: ", errmsg);
	}
	lua_pop(thread, nret);  /* dicard yielded values */
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
	pushhandlemap(L);
	lua_pushthread(L);
	if (lua_gettable(L, -2) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (Operation *)lua_newuserdatauv(L, sizeof(Operation), 0);
		lua_settable(L, -4);
		op->flags = FLAG_REQUEST;
		request = torequest(op);
		request->type = UV_UNKNOWN_REQ;
		request->data = (void *)L;
	}
	else op = (Operation *)lua_touserdata(L, -1);
	lua_pop(L, 2);  /* pops 'Operation' and 'handlemap' */
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

static int k_endop (lua_State *L, int status, lua_KContext kctx) {
	Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg);
	lcu_assert(status == LUA_YIELD);
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
		return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_endop);
	}
	else cancelop(op);
	return nret;
}

#define startreqop(L,S,O,F) startedop(L, S, O, (F)(L, torequest(O), lcu_toloop(S)))
#define startthrop(L,S,O,F,U) startedop(L, S, O, (F)(L, tohandle(O), U))

static int k_resetop (lua_State *L, int status, lua_KContext kctx) {
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
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_resetop);
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


LCUI_FUNC uv_loop_t *lcu_toloop (lcu_Scheduler *sched) {
	return &sched->loop;
}

LCUI_FUNC int lcu_shallsuspend (lcu_Scheduler *sched) {
	return sched->nactive == 0 && sched->nasync > 0;
}

LCUI_FUNC void lcuU_checksuspend (uv_loop_t *loop) {
	lua_State *L = (lua_State *)loop->data;
	lcu_Scheduler *sched = lcu_tosched(loop);
	if (lcu_shallsuspend(sched)) {
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
	lcuU_checksuspend(loop);
	return NULL;
}

LCUI_FUNC void lcuU_resumereqop (uv_loop_t *loop, uv_req_t *request, int narg) {
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)request->data;
	Operation *op = (Operation *)request;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	resumethread(thread, L, narg, loop);
	if (lcuL_maskflag(op, FLAG_REQUEST) && request->type == UV_UNKNOWN_REQ)
		lcuT_freevalue(L, (void *)request);
	lcuU_checksuspend(loop);
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
	lcuU_checksuspend(handle->loop);
	return 0;
}

LCUI_FUNC void lcuU_resumethrop (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	Operation *op = (Operation *)handle;
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	resumethread(thread, L, narg, loop);
	if (!lcuL_maskflag(op, FLAG_PENDING)) cancelop(op);
	lcuU_checksuspend(loop);
}

/*
 * object operation
 */

static void closedobj (uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lcuT_freevalue(L, (void *)handle);  /* becomes garbage */
}

LCUI_FUNC void lcuT_closeobjhdl (lua_State *L, int idx, uv_handle_t *handle) {
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		int nret;
		lua_pushnil(L);
		lua_setiuservalue(L, idx, 1);  /* allow thread to be collected */
		lua_pushnil(thread);
		lua_pushliteral(thread, "closed");
		lua_resume(thread, L, 2, &nret);  /* explicit resume to cancel operation */
		lua_pop(thread, nret);  /* dicard yielded values */
	}
	lua_pushvalue(L, idx);
	lcuT_savevalue(L, (void *)handle);
	uv_close(handle, closedobj);
}

static void stopobjop (lua_State *L, lcu_Object *obj) {
	uv_handle_t *handle = lcu_toobjhdl(obj);
	lcu_ObjectAction stop = lcu_getobjstop(obj);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	int err = stop(handle);
	lcuT_pushsaved(L, handle);  /* restore saved object being stopped */
	lua_pushnil(L);
	lua_setiuservalue(L, -2, 1);  /* allow thread to be collected */
	if (err < 0) {
		lcuT_closeobj(L, -1);
		lcuL_warnerr(L, "object:stop: ", err);
	}
	else lcuT_freevalue(L, (void *)handle);
	lua_pop(L, 1);  /* discard restored saved object */
	lcu_setobjstop(obj, NULL);
	lcu_setobjstep(obj, NULL);
	sched->nactive--;
}

static int k_endobjop (lua_State *L, int status, lua_KContext ctx);

static int scheduledyield (lua_State *L, uv_handle_t *handle) {
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_has_ref(handle));
	lcu_assert(!uv_is_closing(handle));
	lua_pushthread(L);
	lua_setiuservalue(L, 1, 1);
	handle->data = (void *)L;
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_endobjop);
}

static int k_endobjop (lua_State *L, int status, lua_KContext ctx) {
	lcu_Object *obj = (lcu_Object *)lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_toobjhdl(obj);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(handle->data != NULL);
	handle->data = NULL;
	if (!haltedop(L, handle->loop)) {
		lua_CFunction step = lcu_getobjstep(obj);
		int nret = step(L);
		if (nret >= 0) return nret;
		return scheduledyield (L, handle);
	}
	stopobjop(L, obj);
	return lua_gettop(L)-((int)ctx);
}

LCUI_FUNC int lcuT_resetobjopk (lua_State *L,
                                lcu_Object *obj,
                                lcu_ObjectAction start,
                                lcu_ObjectAction stop,
                                lua_CFunction step) {
	uv_handle_t *handle = lcu_toobjhdl(obj);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	luaL_argcheck(L, handle->data == NULL, 1, "already in use");
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (lcu_getobjstop(obj) == NULL) {
		int err;
		lua_pushvalue(L, 1);
		lcuT_savevalue(L, (void *)handle);
		err = start(handle);
		if (err < 0) return lcuL_pusherrres(L, err);
		lcu_setobjstop(obj, stop);
		sched->nactive++;
	}
	lcu_setobjstep(obj, step);
	return scheduledyield (L, handle);
}

LCUI_FUNC void lcuU_resumeobjop (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	resumethread(thread, L, narg, loop);
	if (handle->data == NULL) stopobjop(L, lcu_tohdlobj(handle));
	lcuU_checksuspend(loop);
}
