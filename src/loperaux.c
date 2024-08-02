#include "lmodaux.h"
#include "loperaux.h"

#include <string.h>


#define FLAG_REQUEST  0x01
#define FLAG_THRSAVED  0x02
#define FLAG_PENDING  0x04
#define FLAG_CLEANUP  0x08

#define torequest(O) ((uv_req_t *)&((O)->kind.request))
#define tohandle(O) ((uv_handle_t *)&((O)->kind.handle))

struct lcu_Scheduler {
	uv_loop_t loop;
	int nasync;  /* number of active 'uv_async_t' handles */
	int nactive;  /* number of all active handles */
};


static void pushopmap (lua_State *L) {
	lua_pushlightuserdata(L, pushopmap);
	if (lua_gettable(L, LUA_REGISTRYINDEX) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushlightuserdata(L, pushopmap);
		lua_pushvalue(L, -2);
		lua_createtable(L, 0, 1);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
}

static void closehandle (uv_handle_t* handle, void* arg) {
	(void)arg;
	if (!uv_is_closing(handle)) uv_close(handle, NULL);
}

static int terminateloop (lua_State *L) {
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, 1);
	uv_loop_t *loop = lcu_toloop(sched);
	int err;
	lcu_assert(loop->data == NULL);
	err = uv_loop_close(loop);
	if (err == UV_EBUSY) {
		lcu_log(NULL, L, "unable to close UV loop, closing handles...");
		uv_walk(loop, closehandle, NULL);
		loop->data = (void *)L;
		err = uv_run(loop, UV_RUN_DEFAULT);
		loop->data = NULL;
		if (err >= 0) err = uv_loop_close(loop);
		else {
			lcu_log(NULL, L, "failure: unable to close UV loop!");
			lcuL_warnerr(L, "system.run", err);
			uv_print_all_handles(loop, stderr);
		}
	}
	lcu_assert(err >= 0);
	lcu_log(NULL, L, "UV loop closed");
	return 0;
}

LCUI_FUNC void lcuM_newmodupvs (lua_State *L) {
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_newuserdatauv(L, sizeof(lcu_Scheduler), 0);
	uv_loop_t *loop = lcu_toloop(sched);
	int err = uv_loop_init(loop);
	if (err < 0) lcu_error(L, err);
	sched->nasync = 0;
	sched->nactive = 0;
	loop->data = NULL;
	lcuL_setfinalizer(L, terminateloop);
}

static void savevalue (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_insert(L, -2);
	lua_settable(L, LUA_REGISTRYINDEX);
}

static void pushsaved (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_gettable(L, LUA_REGISTRYINDEX);
}

static void freevalue (lua_State *L, void *key) {
	lua_pushlightuserdata(L, key);
	lua_pushnil(L);
	lua_settable(L, LUA_REGISTRYINDEX);
}

static void savethread (lua_State *L, void *key) {
	lua_pushthread(L);
	savevalue(L, key);
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
		lcuL_warnmsg(L, "system.run", errmsg);
		lua_pop(thread, 1);
	}
	else lua_pop(thread, nret);  /* dicard yielded values */
}

static int haltedop (lua_State *L, void *token) {
	if (lua_touserdata(L, -1) == token) {
		lua_pop(L, 1);  /* discard token */
		return 0;
	}
	return 1;
}

static void checkyieldable (lua_State *L) {
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
}

struct lcu_Operation {
	union {
		union uv_any_req request;
		union uv_any_handle handle;
	} kind;
	int flags;
	lua_CFunction results;
	lua_CFunction cancel;
};

static lcu_Operation *tothrop (lua_State *L) {
	lcu_Operation *op;
	pushopmap(L);
	lua_pushthread(L);
	if (lua_gettable(L, -2) == LUA_TNIL) {
		uv_req_t *request;
		lua_pushthread(L);
		op = (lcu_Operation *)lua_newuserdatauv(L, sizeof(lcu_Operation), 1);
		lua_settable(L, -4);
		op->flags = FLAG_REQUEST;
		request = torequest(op);
		request->type = UV_UNKNOWN_REQ;
		request->data = (void *)L;
	}
	else op = (lcu_Operation *)lua_touserdata(L, -1);
	lua_pop(L, 2);  /* pops 'lcu_Operation' and 'opmap' */
	return op;
}

LCUI_FUNC void lcu_setopvalue (lua_State *L) {
	pushopmap(L);
	lua_pushthread(L);
	if (lua_gettable(L, -2) == LUA_TUSERDATA) {
		lua_pushvalue(L, -3);
		lua_setiuservalue(L, -2, 1);
	}
	else lcu_assert(0);
	lua_pop(L, 3);  /* pops 'lcu_Operation', 'opmap' and 'value' */
}

LCUI_FUNC int lcu_pushopvalue (lua_State *L) {
	int ltype;
	pushopmap(L);
	lua_pushthread(L);
	ltype = lua_gettable(L, -2);
	lua_remove(L, -2);  /* opmap */
	if (ltype == LUA_TUSERDATA) {
		ltype = lua_getiuservalue(L, -1, 1);
		lua_remove(L, -2);  /* lcu_Operation */
	}
	else lcu_assert(ltype == LUA_TNIL);
	return ltype;
}

static void closedhdl (uv_handle_t *handle) {
	lcu_Operation *op = (lcu_Operation *)handle;
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	uv_req_t *request = torequest(op);
	lcu_Scheduler *sched = lcu_tosched(loop);
	lcu_log(op, thread, "closed operation handle");
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST|FLAG_THRSAVED));
	sched->nactive--;
	lcuL_setflag(op, FLAG_REQUEST|FLAG_THRSAVED);
	request->type = UV_UNKNOWN_REQ;
	request->data = thread;
	if (!lcuL_maskflag(op, FLAG_PENDING)) {
		lcuL_clearflag(op, FLAG_THRSAVED);
		freevalue(L, (void *)request);
	}
	else lcuU_resumecoreq(loop, request, 0);
}

static void closehdl (uv_handle_t *handle) {
	if (handle->type == UV_ASYNC) {
		lcu_Scheduler *sched = lcu_tosched(handle->loop);
		sched->nasync--;
	}
	uv_close(handle, closedhdl);
}

static void cancelop (lcu_Operation *op) {
	lcu_assert(!lcuL_maskflag(op, FLAG_CLEANUP));
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		uv_cancel(request);
		lcu_log(op, request->data, "canceled operation request");
	} else {
		uv_handle_t *handle = tohandle(op);
		if (!uv_is_closing(handle)) {
			closehdl(handle);
			lcu_log(op, handle->data, "closing operation handle...");
		}
	}
}

static int k_endop (lua_State *L, int status, lua_KContext kctx) {
	lcu_Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING|FLAG_CLEANUP) == FLAG_PENDING);
	lua_remove(L, narg--);  /* remove 'sched' */
	lcuL_clearflag(op, FLAG_PENDING);
	if (haltedop(L, sched)) {
		lcu_log(op, L, "resumed coroutine");
		if (op->cancel == NULL || op->cancel(L)) cancelop(op);
		else lcuL_setflag(op, FLAG_CLEANUP);
	} else {
		lcu_log(op, L, "resumed operation");
		if (op->results) return op->results(L);
	}
	return lua_gettop(L)-narg; /* return yield */
}

static int startedopk (lua_State *L,
                       lcu_Scheduler *sched,
                       lcu_Operation *op,
                       int nret) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING|FLAG_CLEANUP));
	if (nret < 0) {  /* shall yield, and wait for callback */
		lcuL_setflag(op, FLAG_PENDING);
		lua_pushlightuserdata(L, (void *)sched);
		lcu_log(op, L, "suspended operation");
		return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_endop);
	}
	cancelop(op);
	return nret;
}

#define startcoreqk(L,S,O,F) startedopk(L, S, O, (F)(L, torequest(O), lcu_toloop(S), O))
#define startcohdlk(L,S,O,F,U) startedopk(L, S, O, (F)(L, tohandle(O), U, O))

static int k_resetopk (lua_State *L, int status, lua_KContext kctx) {
	lcu_Operation *op = tothrop(L);
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg-2);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(lcuL_maskflag(op, FLAG_PENDING));
	lcuL_clearflag(op, FLAG_PENDING);
	if (!haltedop(L, sched)) {
		int mkreq = lua_toboolean(L, narg);
		lcu_log(op, L, "resumed operation");
		lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
		lcu_assert(torequest(op)->type == UV_UNKNOWN_REQ);
		if (mkreq) {
			lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg-1);
			lua_settop(L, narg-3);  /* discard yield, 'sched', 'setup' and 'mkreq' */
			return startcoreqk(L, sched, op, setup);
		} else {
			lcu_HandleSetup setup = (lcu_HandleSetup)lua_touserdata(L, narg-1);
			lua_settop(L, narg-3);  /* discard yield, 'sched', 'setup' and 'mkreq' */
			return startcohdlk(L, sched, op, setup, lcu_toloop(sched));
		}
	}
	else lcu_log(op, L, "resumed coroutine");
	return lua_gettop(L)-narg; /* return yield */
}

static int yieldresetk (lua_State *L,
                        lcu_Scheduler *sched,
                        lcu_Operation *op,
                        int mkreq,
                        void *setup) {
	lcu_assert(!lcuL_maskflag(op, FLAG_PENDING));
	lua_pushlightuserdata(L, (void *)sched);
	lua_pushlightuserdata(L, (void *)setup);
	lua_pushboolean(L, mkreq);
	lcuL_setflag(op, FLAG_PENDING);
	lcu_log(op, L, "suspended operation");
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_resetopk);
}

typedef enum { FREEOP, SAMEOP, WAITOP } OpStatus;

static OpStatus checkreset (lcu_Operation *op,
                            lua_CFunction results,
                            lua_CFunction cancel,
                            uv_handle_type type) {
	op->results = results;
	op->cancel = cancel;
	if (lcuL_maskflag(op, FLAG_REQUEST)) {
		uv_req_t *request = torequest(op);
		if (request->type == UV_UNKNOWN_REQ) return FREEOP;
	} else {
		uv_handle_t *handle = tohandle(op);
		int nocleanup = !lcuL_maskflag(op, FLAG_CLEANUP);
		if (nocleanup && !uv_is_closing(handle)) {
			if (type && handle->type == type) return SAMEOP;
			if (nocleanup) closehdl(handle);
		}
	}
	return WAITOP;
}


LCUI_FUNC uv_loop_t *lcu_toloop (lcu_Scheduler *sched) {
	return &sched->loop;
}

LCUI_FUNC int lcu_shallsuspend (lcu_Scheduler *sched) {
	return sched->nasync > 0 && sched->nasync == sched->nactive;
}

LCUI_FUNC void lcuU_checksuspend (uv_loop_t *loop) {
	lua_State *L = (lua_State *)loop->data;
	lcu_Scheduler *sched = lcu_tosched(loop);
	if (lcu_shallsuspend(sched)) {
		if (lua_getfield(L, LUA_REGISTRYINDEX, LCU_CHANNELTASKREGKEY) == LUA_TUSERDATA) {
			uv_stop(loop);
			lcu_log(loop, L, "halting loop");
		}
		else lua_pop(L, 1);
	}
}


/*
 * coroutine request
 */

LCUI_FUNC int lcuT_resetcoreqk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel) {
	lcu_Operation *op = tothrop(L);
	checkyieldable(L);
	switch (checkreset(op, results, cancel, 0)) {
		case FREEOP: return startcoreqk(L, sched, op, setup);
		case WAITOP: return yieldresetk(L, sched, op, 1, (void *)setup);
		default: lcu_assert(0);
	}
	return 0;  /* unreachable */
}

LCUI_FUNC void lcuT_armcoreq (lua_State *L,
                              uv_loop_t *loop,
                              lcu_Operation *op,
                              int err) {
	uv_req_t *request = torequest(op);
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	if (err >= 0) {
		lcu_Scheduler *sched = lcu_tosched(loop);
		if (!lcuL_maskflag(op, FLAG_THRSAVED)) {
			savethread(L, (void *)request);  // TODO: memory error here makes 'lcu_Operation' be GC.
			lcuL_setflag(op, FLAG_THRSAVED);
		}
		sched->nactive++;
	} else {
		request->type = UV_UNKNOWN_REQ;
	}
}

LCUI_FUNC lua_State *lcuU_endcoreq (uv_loop_t *loop, uv_req_t *request) {
	lua_State *L = (lua_State *)loop->data;
	lcu_Operation *op = (lcu_Operation *)request;
	lcu_Scheduler *sched = lcu_tosched(loop);
	sched->nactive--;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST|FLAG_THRSAVED) == (FLAG_REQUEST|FLAG_THRSAVED));
	request->type = UV_UNKNOWN_REQ;
	if (lcuL_maskflag(op, FLAG_PENDING)) return (lua_State *)request->data;
	lcuL_clearflag(op, FLAG_THRSAVED|FLAG_CLEANUP);
	freevalue(L, (void *)request);
	lcuU_checksuspend(loop);
	return NULL;
}

LCUI_FUNC void lcuU_resumecoreq (uv_loop_t *loop, uv_req_t *request, int narg) {
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)request->data;
	lcu_Operation *op = (lcu_Operation *)request;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST|FLAG_THRSAVED|FLAG_PENDING) == (FLAG_REQUEST|FLAG_THRSAVED|FLAG_PENDING));
	lcu_assert(request->type == UV_UNKNOWN_REQ);
	resumethread(thread, L, narg, loop);
	if (lcuL_maskflag(op, FLAG_REQUEST) && request->type == UV_UNKNOWN_REQ) {
		lcuL_clearflag(op, FLAG_THRSAVED);
		freevalue(L, (void *)request);
	}
	lcuU_checksuspend(loop);
}


/*
 * coroutine handle
 */

LCUI_FUNC int lcuT_resetcohdlk (lua_State *L,
                                uv_handle_type type,
                                lcu_Scheduler *sched,
                                lcu_HandleSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel) {
	lcu_Operation *op = tothrop(L);
	uv_loop_t *loop = NULL;
	lcu_assert(type);
	checkyieldable(L);
	switch (checkreset(op, results, cancel, type)) {
		case FREEOP: loop = lcu_toloop(sched); /* FALLTHRU */
		case SAMEOP: return startcohdlk(L, sched, op, setup, loop);
		case WAITOP: return yieldresetk(L, sched, op, 0, (void *)setup);
	}
	return 0;  /* unreachable */
}

LCUI_FUNC int lcuT_armcohdl (lua_State *L, lcu_Operation *op, int err) {
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST));
	if (err >= 0) {
		uv_handle_t *handle = tohandle(op);
		lcu_Scheduler *sched = lcu_tosched(handle->loop);
		if (!lcuL_maskflag(op, FLAG_THRSAVED)) savethread(L, (void *)handle);  // TODO: memory error here makes 'lcu_Operation' be GC.
		handle->data = (void *)L;
		lcuL_clearflag(op, FLAG_REQUEST|FLAG_THRSAVED);
		if (handle->type == UV_ASYNC) sched->nasync++;
		sched->nactive++;
	} else {
		uv_req_t *request = torequest(op);
		request->type = UV_UNKNOWN_REQ;
	}
	return err;
}

LCUI_FUNC int lcuU_endcohdl (uv_handle_t *handle) {
	lcu_Operation *op = (lcu_Operation *)handle;
	lcu_assert(!lcuL_maskflag(op, FLAG_REQUEST));
	if (lcuL_maskflag(op, FLAG_PENDING|FLAG_CLEANUP) == FLAG_PENDING) return 1;
	lcuL_clearflag(op, FLAG_CLEANUP);
	cancelop(op);
	lcuU_checksuspend(handle->loop);
	return 0;
}

LCUI_FUNC void lcuU_resumecohdl (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	lcu_Operation *op = (lcu_Operation *)handle;
	lcu_assert(lcuL_maskflag(op, FLAG_REQUEST|FLAG_PENDING) == FLAG_PENDING);
	resumethread(thread, L, narg, loop);
	if (!lcuL_maskflag(op, FLAG_PENDING|FLAG_CLEANUP)) cancelop(op);
	lcuU_checksuspend(loop);
}

/*
 * userdata handle
 */

#define UPV_THREAD	1
#define UPV_SCHEDULER	2

LCUI_FUNC lcu_UdataHandle *lcuT_createudhdl (lua_State *L,
                                             int schedidx,
                                             size_t sz,
                                             const char *cls) {
	lcu_UdataHandle *udhdl;
	lua_pushvalue(L, schedidx);  /* push scheduler object */
	udhdl = (lcu_UdataHandle *)lua_newuserdatauv(L, sz, 2);
	lcu_assert(sz >= sizeof(lcu_UdataHandle));
	udhdl->flags = LCU_HANDLECLOSEDFLAG;
	udhdl->stop = NULL;
	udhdl->step = NULL;
	udhdl->handle.data = NULL;
	luaL_setmetatable(L, cls);
	lua_insert(L, -2);
	lua_setiuservalue(L, -2, UPV_SCHEDULER);  /* avoid scheduler to be collected */
	return udhdl;
}

static void closedudhdl (uv_handle_t *handle) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	freevalue(L, (void *)handle);  /* becomes garbage */
	lcu_log(handle, L, "closed object handle");
}

LCUI_FUNC int lcu_closeudhdl (lua_State *L, int idx) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, idx);
	if (udhdl && !lcuL_maskflag(udhdl, LCU_HANDLECLOSEDFLAG)) {
		uv_handle_t *handle = lcu_ud2hdl(udhdl);
		lua_State *thread = (lua_State *)handle->data;
		if (thread) {
			int nret, status;
			lua_pushboolean(thread, 0);
			lua_pushliteral(thread, "closed");
			status = lua_resume(thread, L, 2, &nret);  /* explicit resume to cancel operation */
			if (status == LUA_YIELD || status == LUA_OK) {
				lua_pop(thread, nret);  /* dicard yielded values */
			} else {
				const char *err = lua_tostring(thread, -1);
				lcuL_warnmsg(L, "object:close", err);
				lua_pop(thread, 1);
			}
			lua_pushnil(L);
			lua_setiuservalue(L, idx, UPV_THREAD);  /* allow thread to be collected */
		}
		lua_pushvalue(L, idx);
		savevalue(L, (void *)handle);
		uv_close(handle, closedudhdl);
		lcuL_setflag(udhdl, LCU_HANDLECLOSEDFLAG);
		lcu_log(handle, L, "closing object handle");
		return 1;
	}
	return 0;
}

LCUI_FUNC lcu_UdataHandle *lcu_openedudhdl (lua_State *L, int arg, const char *class) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)luaL_checkudata(L, arg, class);
	luaL_argcheck(L, !lcuL_maskflag(udhdl, LCU_HANDLECLOSEDFLAG), arg, "closed object");
	return udhdl;
}

static void stopudhdl (lua_State *L, lcu_UdataHandle *udhdl) {
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	int err = 0;
	if (udhdl->stop) err = udhdl->stop(handle);
	lcu_assert(handle->data == NULL);
	pushsaved(L, (void *)handle);  /* restore saved object being stopped */
	if (err < 0) {
		lcu_closeudhdl(L, -1);
		lcuL_warnerr(L, "object:stop", err);
	} else {
		lua_pushnil(L);
		lua_setiuservalue(L, -2, UPV_THREAD);
		if (!lcuL_maskflag(udhdl, LCU_HANDLECLOSEDFLAG)) freevalue(L, (void *)handle);
	}
	lua_pop(L, 1);  /* discard restored saved object */
	udhdl->stop = NULL;
	udhdl->step = NULL;
	sched->nactive--;
}

static int k_endudhdlk (lua_State *L, int status, lua_KContext kctx);

static int scheduleudhdlk (lua_State *L, uv_handle_t *handle) {
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_has_ref(handle));
	lcu_assert(!uv_is_closing(handle));
	lua_pushthread(L);
	lua_setiuservalue(L, 1, UPV_THREAD);
	handle->data = (void *)L;
	lcu_log(handle, L, "suspended operation");
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_endudhdlk);
}

static int k_endudhdlk (lua_State *L, int status, lua_KContext kctx) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(handle->data != NULL);
	handle->data = NULL;
	if (!haltedop(L, handle->loop)) {
		int nret = udhdl->step(L);
		lcu_log(handle, L, "resumed operation");
		if (nret >= 0) return nret;
		return scheduleudhdlk (L, handle);
	}
	stopudhdl(L, udhdl);
	lcu_log(handle, L, "resumed coroutine");
	return lua_gettop(L)-((int)kctx);
}

LCUI_FUNC int lcuT_resetudhdlk (lua_State *L,
                                lcu_UdataHandle *udhdl,
                                lcu_HandleAction start,
                                lcu_HandleAction stop,
                                lua_CFunction step) {
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	luaL_argcheck(L, handle->data == NULL, 1, "already in use");
	checkyieldable(L);
	udhdl->step = step;
	if (udhdl->stop == NULL) {  /* 'handle' was started, calling op again */
		int err;
		lua_pushvalue(L, 1);
		savevalue(L, (void *)handle);  /* save now, because it may raise memory error */
		handle->data = (void *)L;  /* this is eventually done if 'start' returns no error, */
		                           /* but libuv might call 'uv_alloc_cb' inside 'uv_*_start', */
		                           /* therefore we must set everything up prematurely. */
		                           /* Ref.: https://groups.google.com/g/libuv/c/hWqdJ35jafk/m/VuZgalktAQAJ */
		err = start(handle);
		if (err < 0) {
			udhdl->step = NULL; 
			handle->data = NULL;  /* rollback the premature setup for callbacks (see above) */
			freevalue(L, (void *)handle);
			return lcuL_pusherrres(L, err);
		}
		udhdl->stop = stop;
		sched->nactive++;
	}
	return scheduleudhdlk (L, handle);
}

LCUI_FUNC void lcuU_resumeudhdl (uv_handle_t *handle, int narg) {
	uv_loop_t *loop = handle->loop;
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)handle->data;
	lua_pushthread(thread);
	lua_xmove(thread, L, 1);  /* save thread in case it is replaced in UPV_THREAD */
	resumethread(thread, L, narg, loop);
	lua_pop(L, 1);
	if (handle->data == NULL) stopudhdl(L, lcu_hdl2ud(handle));
	lcuU_checksuspend(loop);
}


/*
 * userdata request
 */

LCUI_FUNC lcu_UdataRequest *lcuT_createudreq (lua_State *L, size_t sz) {
	lcu_UdataRequest *udreq = (lcu_UdataRequest *)lua_newuserdatauv(L, sz, 1);
	lcu_assert(sz >= sizeof(lcu_UdataRequest));
	udreq->request.type = UV_UNKNOWN_REQ;
	udreq->request.data = NULL;
	return udreq;
}

static void freeudreq (lua_State *L, uv_req_t *request) {
	lcu_assert(request->type == UV_REQ_TYPE_MAX);
	pushsaved(L, (void *)request);  /* restore saved object being stopped */
	lua_pushnil(L);
	lua_setiuservalue(L, -2, UPV_THREAD);
	request->data = NULL;
	freevalue(L, (void *)request);
	request->type = UV_UNKNOWN_REQ;
	lua_pop(L, 1);  /* discard restored saved object */
}

static int k_endudreq (lua_State *L, int status, lua_KContext kctx) {
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg);
	lcu_UdataRequest *udreq = (lcu_UdataRequest *)lua_touserdata(L, 1);
	uv_req_t *request = lcu_ud2req(udreq);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(request->data == L);
	lua_remove(L, narg--);  /* remove 'sched' */
	request->data = NULL;  /* mark 'udreq' as free */
	if (haltedop(L, sched)) {
		lua_pushnil(L);
		lua_setiuservalue(L, 1, UPV_THREAD);
		lcu_log(request, L, "resumed coroutine");
		if (udreq->cancel == NULL || udreq->cancel(L)) uv_cancel(request);
	} else {
		lcu_log(request, L, "resumed operation");
		if (udreq->results) return udreq->results(L);
	}
	return lua_gettop(L)-narg;
}

static int startedudreqk (lua_State *L,
                          lcu_Scheduler *sched,
                          lcu_UdataRequest *udreq,
                          int nret,
                          lua_CFunction results,
                          lua_CFunction cancel) {
	uv_req_t *request = lcu_ud2req(udreq);
	if (nret >= 0) {
		freeudreq(L, request);
		return nret;
	}
	lcu_assert(request->type != UV_UNKNOWN_REQ);
	lcu_assert(request->type != UV_REQ_TYPE_MAX);
	lcu_assert(request->data == L);
	sched->nactive++;
	udreq->results = results;
	udreq->cancel = cancel;
	lua_pushlightuserdata(L, (void *)sched);
	lcu_log(request, L, "suspended operation");
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_endudreq);
}

static int k_resetudreqk (lua_State *L, int status, lua_KContext kctx) {
	int narg = (int)kctx;
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, narg-3);
	lcu_UdataRequest *udreq = (lcu_UdataRequest *)lua_touserdata(L, 1);
	uv_req_t *request = lcu_ud2req(udreq);
	lcu_assert(status == LUA_YIELD);
	if (!haltedop(L, sched)) {
		int nret;
		uv_loop_t *loop = lcu_toloop(sched);
		lcu_RequestSetup setup = (lcu_RequestSetup)lua_touserdata(L, narg-2);
		lua_CFunction results = (lua_CFunction)lua_touserdata(L, narg-1);
		lua_CFunction cancel = (lua_CFunction)lua_touserdata(L, narg);
		lcu_assert(request->type == UV_UNKNOWN_REQ);
		lua_settop(L, narg-4);  /* discard yield, 'sched', 'setup', 'results' and 'cancel' */
		lcu_log(request, L, "resumed operation");
		nret = setup(L, request, loop, NULL);
		return startedudreqk(L, sched, udreq, nret, results, cancel);
	}
	lua_pushnil(L);
	lua_setiuservalue(L, 1, UPV_THREAD);
	request->data = NULL;
	lcu_log(request, L, "resumed coroutine");
	return lua_gettop(L)-narg; /* return yield */
}

LCUI_FUNC int lcuT_resetudreqk (lua_State *L,
                                lcu_Scheduler *sched,
                                lcu_UdataRequest *udreq,
                                lcu_RequestSetup setup,
                                lua_CFunction results,
                                lua_CFunction cancel) {
	uv_req_t *request = lcu_ud2req(udreq);
	luaL_argcheck(L, request->data == NULL, 1, "already in use");
	checkyieldable(L);
	if (request->type == UV_UNKNOWN_REQ) {  /* 'request' is free and collectable */
		lua_pushvalue(L, 1);
		savevalue(L, (void *)request);  /* save now, because it may raise memory error */
		request->type = UV_REQ_TYPE_MAX;  /* 'request' is free but not collectable */
	}
	lua_pushthread(L);
	lua_setiuservalue(L, 1, UPV_THREAD);
	request->data = (void *)L;
	if (request->type == UV_REQ_TYPE_MAX) {  /* while previous call returned */
		uv_loop_t *loop = lcu_toloop(sched);
		int nret = setup(L, request, loop, NULL);
		return startedudreqk(L, sched, udreq, nret, results, cancel);
	}
	/* previous caller was resumed while request was still pending */
	lua_pushlightuserdata(L, (void *)sched);
	lua_pushlightuserdata(L, (void *)setup);
	lua_pushlightuserdata(L, (void *)results);
	lua_pushlightuserdata(L, (void *)cancel);
	lcu_log(request, L, "suspended operation");
	return lua_yieldk(L, 0, (lua_KContext)lua_gettop(L), k_resetudreqk);
}

LCUI_FUNC lua_State *lcuU_endudreq (uv_loop_t *loop, uv_req_t *request) {
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)request->data;
	lcu_Scheduler *sched = lcu_tosched(loop);
	lcu_assert(request->type != UV_UNKNOWN_REQ);
	lcu_assert(request->type != UV_REQ_TYPE_MAX);
	request->type = UV_REQ_TYPE_MAX;
	sched->nactive--;
	if (thread) return thread;
	freeudreq(L, request);
	lcuU_checksuspend(loop);
	return NULL;
}

LCUI_FUNC void lcuU_resumeudreq (uv_loop_t *loop, uv_req_t *request, int narg) {
	lua_State *L = (lua_State *)loop->data;
	lua_State *thread = (lua_State *)request->data;
	lua_pushthread(thread);
	lua_xmove(thread, L, 1);  /* save thread in case it is replaced in UPV_THREAD */
	resumethread(thread, L, narg, loop);
	lua_pop(L, 1);
	if (request->type == UV_REQ_TYPE_MAX) freeudreq(L, request);
	lcuU_checksuspend(loop);
}

/*
 * auxiliary functions
 */

LCUI_FUNC int lcuL_checknoyieldmode (lua_State *L, int arg) {
	const char *mode = luaL_optstring(L, arg, "");
	int i;
	for (i = 0; mode[i]; i++)
		if (mode[i] != LCU_NOYIELDMODE)
			return luaL_error(L, "unknown mode char (got '%c')", mode[i]);
	return i;
}
