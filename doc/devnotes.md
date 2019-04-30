Terminology
===========

Operations
----------

CoUtil categorizes UV asynchronous operations into the following categories:

### Request Operations (`reqop`)

Asynchronous operation perfomed over a UV request.

| Operation | Request | Callback |
| --------- | ------- | -------- |
| `tcp:send` | `UV_WRITE` | `uv_write_cb` |
| `tcp:connect` | `UV_CONNECT` | `uv_connect_cb` |
| `tcp:shutdown` | `UV_SHUTDOWN` | `uv_shutdown_cb` |
| --------- | ------- | -------- |
| `udp:send` | `UV_UDP_SEND` | `uv_udp_send_cb` |
| `system.toaddress` | `UV_GETADDRINFO` | `uv_getaddrinfo_cb` |
| `system.tonetname` | `UV_GETNAMEINFO` | `uv_getnameinfo_cb` |
| `file:*` | `UV_FS` | `uv_fs_cb` |

### Thread Operations (`throp`)

Asynchronous operation perfomed over a UV handle that can be replicated for different threads resulting in the same effect.

| Operation | Handle | Callback |
| --------- | ------ | -------- |
| `system.pause` | `UV_IDLE` | `uv_idle_cb` |
| `system.pause` | `UV_TIMER` | `uv_timer_cb` |
| `system.awaitsig` | `UV_SIGNAL` | `uv_signal_cb` |
| --------- | ------ | -------- |
| `system.awaitpath` | `UV_FS_EVENT` | `uv_fs_event_cb` |
| `system.awaitpath` | `UV_FS_POLL` | `uv_fs_poll_cb` |

### Object Operations (`objop`)

Asynchronous operation perfomed over a UV handle associated with a system resource with is own identity that cannot be replicated for each thread (_e.g._ socket, pipe or TTY file).

| Operation | Handle | Callback |
| --------- | ------ | -------- |
| `tcp:receive` | `UV_TCP` | `uv_read_cb` |
| `tcp:accept` | `UV_TCP` | `uv_connection_cb` |
| --------- | ------ | -------- |
| `udp:receive` | `UV_UDP` | `uv_udp_recv_cb` |
| `pipe:receive` | `UV_NAMED_PIPE` | `uv_read_cb` |
| `pipe:accept` | `UV_NAMED_PIPE` | `uv_connection_cb` |
| `tty:receive` | `UV_TTY` | `uv_read_cb` |
| `tty:accept` | `UV_TTY` | `uv_connection_cb` |
| `process:exitval` | `UV_PROCESS` | `uv_exit_cb` |

Subject
-------

| Abbrev. | Name | Description |
| ------- | ---- | ----------- |
| `loop` | `loop` | [UV loop](http://docs.libuv.org/en/v1.x/loop.html) |
| `hdl` | `handle` | [UV handle](http://docs.libuv.org/en/v1.x/handle.html) |
| `req` | `request` | [UV request](http://docs.libuv.org/en/v1.x/request.html) |
| `thr` | `thread` | [Lua coroutine](http://www.lua.org/manual/5.3/manual.html#2.6) |
| `kctx` | `kcontext` | [Lua continuation context](http://www.lua.org/manual/5.3/manual.html#lua_KContext) |
| `kfn` | `kfunction` | [Lua continuation function](http://www.lua.org/manual/5.3/manual.html#lua_KFunction) |
| `idx` | `index` | [Lua stack index](http://www.lua.org/manual/5.3/manual.html#4.3) |
| `arg` | `argument` | [Lua function argument position](http://www.lua.org/manual/5.3/manual.html#lua_CFunction) |
| `obj` | `object` | System resource representation (_e.g._ socket, file, process) |
| `flag` | `flags` | [Bit field](https://en.wikipedia.org/wiki/Bit_field) |
| `op` | `operation` | Asynchronous operation over [UV library](https://libuv.org/). |

Decorations
-----------

| Decoration | Type | Description |
| ---------- | ---- | ----------- |
| `lcu_` | `prefix` | API for manipulation of CoUtil values |
| `lcuL_` | `prefix` | API with utilility Lua functions |
| `lcuT_` | `prefix` | API for thread operations (**require module upvalues**). |
| `lcuU_` | `prefix` | API for UV callbacks (**require module upvalues**). |
| `lcuB_` | `prefix` | UV callback function. |
| `lcuK_` | `prefix` | Lua continuation function. |
| `k` | `suffix` | Function that yields with a continuation. |

States
======

Thread
------

![Thread States](thrstate.svg)

### Resources

- operation:
```c
/* from Lua stack (inside module closure) */
lcu_Operation *operation = lcuT_tothrop(L);
/* from UV handle */
lcu_Operation *operation = (lcu_Operation *)handle;
/* from UV request */
lcu_Operation *operation = (lcu_Operation *)request;
```
- handle/request:
```c
if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) {
	uv_req_t *request = lcu_torequest(operation);
} else {
	uv_handle_t *handle = lcu_tohandle(operation);
}
```
- coroutine:
```c
lua_State *thread = (lua_State *)( lcu_testflag(op, LCU_OPFLAG_REQUEST) ?
                                   lcu_torequest(op)->data :
                                   lcu_tohandle(op)->data );
```

### Conditions

- Freed:
```c
/* coroutine notification pending */
lcu_assert(lcu_testflag(operation, LCU_OPFLAG_REQUEST));
lcu_assert(!lcu_testflag(operation, LCU_OPFLAG_PENDING));
/* no armed operation */
uv_req_t *request = lcu_torequest(operation);
lcu_assert(request->type == UV_UNKNOWN_REQ);
/* coroutine is subject to garbage collection */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TNIL);
```
- Armed:
```c
/* coroutine notification pending */
lcu_assert(!lcu_testflag(operation, LCU_OPFLAG_REQUEST));
lcu_assert(!lcu_testflag(operation, LCU_OPFLAG_PENDING));
/* no armed operation */
uv_handle_t *handle = lcu_tohandle(operation);
lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
lcu_assert(!uv_is_closing(handle));
/* LCU_COREGISTRY[&operation] == coroutine */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```
- Await:
```c
/* coroutine notification pending */
lcu_assert(lcu_testflag(operation, LCU_OPFLAG_PENDING));
if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) {
	/* armed request operation */
	uv_req_t *request = lcu_torequest(operation);
	lcu_assert(request->type != UV_UNKNOWN_REQ);
} else {
	/* armed thread operation */
	uv_handle_t *handle = lcu_tohandle(operation);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(!uv_is_closing(handle));
}
/* LCU_COREGISTRY[&operation] == coroutine */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```
- Close:
```c
/* coroutine notification pending */
if (lcu_testflag(op, LCU_OPFLAG_REQUEST)) {
	/* armed request operation */
	uv_req_t *request = lcu_torequest(operation);
	lcu_assert(request->type != UV_UNKNOWN_REQ);
} else {
	/* thread operation close pending */
	uv_handle_t *handle = lcu_tohandle(operation);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_is_closing(handle));
}
/* LCU_COREGISTRY[&operation] == coroutine */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```

### Transitions

|  # |  P | S | Action | Condition |
| -- | -- | - | ------ | --------- |
|  1 |    | T | `lcuT_resetopk+lcuT_awaitopk` | UV request started |
|  2 |    | T | `lcuT_resetopk+lcuT_armthrop` | UV handle initialized |
|  3 |    | U | `lcuU_endreqop` | resumed by UV request callback |
|  4 |    | T | `lcuT_doneop` | resumed by `coroutine.resume` |
|  5 |  2 | T | `lcu_freethrop` | UV handle start failed |
|  5 | 10 | U | `lcu_freethrop` | coroutine suspended or terminated |
|  5 | 10 | T | `lcuT_resetopk` | different UV handle is active |
|  6 |  2 | T | `lcuT_awaitopk` | UV handle started |
|  6 | 10 | T | `lcuT_awaitopk` | coroutine repeats operation |
|  7 |    | U | `lcuU_endreqop` | UV request concluded |
|  8 |    | U | `lcuU_closedhdl` | UV handle closed |
|  9 |    | T | `lcuT_donethrop` | resumed by `coroutine.resume` |
| 10 |    | U | `lcuU_endthrop` | resumed by UV handle callback |
_______________________
P = Previous transition
S = Call scope (T = Thread; U = UV loop)

Object
------

![Object States](objstate.svg)

### Resources

- `object`:
```c
/* from Lua stack */
lcu_##OBJTYPE *object = (lcu_##OBJTYPE *)luaL_testudata(L, arg, OBJMETATABLE);
/* from UV handle */
lcu_##OBJTYPE *object = (lcu_##OBJTYPE *)handle;
```
- `handle`:
```c
uv_handle_t *handle = &object->handle;
```
- `flags`:
```c
if (lcu_testflag(object, LCU_OBJFLAG_CLOSED))
	lcu_clearflag(object, LCU_OBJFLAG_CLOSED);
else
	lcu_setflag(object, LCU_OBJFLAG_CLOSED);
```

### Conditions

- Ready:
```c
/* not being closed */
lcu_assert(!lcu_testflag(object, LCU_OBJFLAG_CLOSED));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* object is subject to garbage collection */
```
- Await:
```c
/* not being closed */
lcu_assert(!lcu_testflag(object, LCU_OBJFLAG_CLOSED));
/* awaiting coroutine */
lua_State *thread = (lua_State *)handle->data;
lcu_assert(thread);
/* object in coroutine stack */
/* LCU_COREGISTRY[&handle] == coroutine */
lua_pushlightuserdata(L, (void *)handle);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```
- Armed:
```c
/* not being closed */
lcu_assert(!lcu_testflag(object, LCU_OBJFLAG_CLOSED));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* object in coroutine stack */
/* LCU_COREGISTRY[&handle] == coroutine */
lua_pushlightuserdata(L, (void *)handle);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```
- Close:
```c
/* not being closed */
lcu_assert(lcu_testflag(object, LCU_OBJFLAG_CLOSED));
/* object is being closed */
lcu_assert(uv_is_closing(handle));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* LCU_COREGISTRY[&handle] == object */
lua_pushlightuserdata(L, (void *)handle);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TLIGHTUSERDATA);
lcu_assert(lua_touserdata(L, -1) == object);
```
- Freed:
```c
/* not being closed */
lcu_assert(lcu_testflag(object, LCU_OBJFLAG_CLOSED));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* object is subject to garbage collection */
```

### Transitions

| # | P | S | Action | Condition |
| - | - | - | ------ | --------- |
| 1 |   | T | `lcu_closeobj` | object closed or collected |
| 2 |   | T | `lcuT_awaitobjk` | UV handle started |
| 3 |   | U | `lcuU_closedobj` | UV handle closed |
| 4 |   | T | `!lcuT_doneop && lcu_releaseobj` | resumed by `coroutine.resume` |
| 5 |   | T | `lcu_closeobj` | object closed or collected |
| 6 |   | U | `lcuU_endobjop` | resumed by UV handle callback |
| 7 | 6 | U | `lcu_releaseobj` | coroutine suspended or terminated |
| 8 | 6 | T | `lcu_closeobj` | object closed or collected |
| 9 | 6 | T | `lcuT_awaitobjk` | coroutine repeated operation |
_______________________
P = Previous transition
S = Call scope (T = Thread; U = UV loop)

Code
====

```c
#define lcu_testflag(V,B)  ((V)->flags&(B))
#define lcu_setflag(V,B)  ((V)->flags |= (B))
#define lcu_clearflag(V,B)  ((V)->flags &= ~(F))

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

LCULIB_API lcu_Operation *lcuT_tothrop (lua_State *L) {
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

LCULIB_API lcu_Operation *lcuT_resetopk (lua_State *L,
                                         int request,
                                         int type,
                                         lua_KContext kctx,
                                         lua_KFunction setup) {
	lcu_Operation *op = lcuT_tothrop(L);
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

LCULIB_API void lcu_freethrop (lcu_Operation *op) {
	uv_handle_t *handle = lcu_tohandle(op);
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_PENDING));
	lcu_assert(!lcu_testflag(op, LCU_OPFLAG_REQUEST));
	if (!uv_is_closing(handle)) uv_close(handle, lcuB_closedhdl);
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
```