Terminology
===========

Operations
----------

CoUtil categorizes UV asynchronous operations into the following categories:

### Request Operations (`reqop`)

- Requires a UV request ([`uv_req_t`](http://docs.libuv.org/en/v1.x/request.html)).
- Requires an initialized `uv_loop_t` structure.
- Is started using a UV function that register a single-shot callback, which is eventually called and indicates the UV request is freed.

| Definition | Description |
| :--- | :--- |
| `uv_<name>_t` | UV request type |
| `uv_<name>_cb` | callback pointer type, which is called when the operation terminates and the request is free. |
| `uv_<name>(uv_loop_t *loop, uv_<name>_t *req, <arguments>, uv_<name>_cb callback);` | starts the operation on UV event loop `loop`. |

### Thread Operations (`throp`)

- Requires a UV handle ([`uv_handle_t`](http://docs.libuv.org/en/v1.x/handle.html)) not associated with a system resource, like a file descriptor.
- Must be first initialized using a initialized `uv_loop_t` structure, before it can start.
- Is started using a UV function that registers a callback that will be continuously called until the operation is stopped using another UV function.
- Is terminated by function `uv_close`,
which registers an additional callback to be called when the UV handle is released.

| Definition | Description |
| :--- | :--- |
| `uv_<name>_t` | UV handle type |
| `uv_<name>_cb` | callback pointer type, which is called while the operation is active. |
| `uv_<name>_init(uv_loop_t *loop, uv_<name>_t *handle);` | associates the handler with UV event loop `loop`. |
| `uv_<name>_start(uv_<name>_t *handle, <arguments>, uv_<name>_cb callback);` | starts the operation with the provided arguments, and registering the callback. |
| `uv_<name>_stop(uv_<name>_t *handle);` | stops the operation, so the callback is not called until started again. |

### Object Operations (`objop`)

Same as [Thread Operations],
but when the handle is associated with a system resource, like a file descriptor.
This distinction is important because such handles are not replicated to allow multiple coroutines to perform its operation simultaneously.

Identifiers
-----------

| Short | Long | Description |
| ----- | ---- | ----------- |
| `loop` | `loop` | [UV loop](http://docs.libuv.org/en/v1.x/loop.html) |
| `hdl` | `handle` | [UV handle](http://docs.libuv.org/en/v1.x/handle.html) |
| `req` | `request` | [UV request](http://docs.libuv.org/en/v1.x/request.html) |
| `thr` | `thread` | [Lua coroutine](http://www.lua.org/manual/5.3/manual.html#2.6) |
| `idx` | `index` | [Lua stack index](http://www.lua.org/manual/5.3/manual.html#4.3) |
| `arg` | `argument` | [Lua function argument position](http://www.lua.org/manual/5.3/manual.html#lua_CFunction) |
| `obj` | `object` | System resource representation (_e.g._ socket, file, process) |
| `op` | `operation` | Asynchronous operation over [UV library](https://libuv.org/). |

Functions
---------

| Decoration | Type | Description |
| ---------- | ---- | ----------- |
| `lcu_` | `prefix` | API for manipulation of CoUtil values |
| `lcuL_` | `prefix` | API with utilility Lua functions |
| `lcuU_` | `prefix` | API for UV callbacks (see note 1). |
| `lcuT_` | `prefix` | API for thread operations (see note 2). |
| `uv_on` | `prefix` | UV callback function. |
| `k_` | `prefix` | Lua continuation function. |
| `k` | `suffix` | Function that yields with a continuation. |

### Notes

1. Requires an active `uv_loop_t` (**while `system.run()` is running**).
2. Requires `lua_State` of a call to a function with **module upvalues**.
Can be used with `(lua_State *)uv_loop_t.data` while `system.run()` is running.

Files
=====

### `lcuconf.h`

Definitions to customize implementation in similar fashion to `luaconf.h` of Lua.

### `lmodaux.c`:

Auxiliary functions for implementation of Lua modules.

### `loperaux.c`: 

Auxiliary functions for implementation of module operations,
specially [await functions](manual.md#await).

States
======

Thread
------

![Thread States](thrstates.svg)

### States

| Name                           | E | R | S | P | C | KFunction    | Callback Pending  |
|:------------------------------ |:-:|:-:|:-:|:-:|:-:|:------------ |:----------------  |
| Freed              Operation   | ? | R |   |   |   |              |                   |
| Awaiting           Request Op. |   | R | S | P |   | `k_endop`    | `uv_<request>_cb` |
| Completed          Request Op. | E | R | S |   |   |              |                   |
| Canceled           Request Op. | ? | R | S |   | ? |              | `uv_<request>_cb` |
| Reseting           Request Op. |   | R | S | P | ? | `k_resetopk` | `uv_<request>_cb` |
| Awaiting           Thread Op.  |   |   |   | P |   | `k_endop`    | `uv_<handle>_cb`  |
| Completed          Thread Op.  | E |   |   |   |   |              | `uv_<handle>_cb`  |
| Canceled           Thread Op.  | ? |   |   |   | C |              | `uv_<handle>_cb`  |
| Reseting  Canceled Thread Op.  |   |   |   | P | C | `k_resetopk` | `uv_<handle>_cb`  |
| Closing            Thread Op.  | ? |   |   |   |   |              | `uv_close_cb`     |
| Reseting  Closed   Thread Op.  |   |   |   | P |   | `k_resetopk` | `uv_close_cb`     |
_______________________
- E: coroutine is executing.
- R: `FLAG_REQUEST` is set in coroutines's `Operation.flags`.
- S: `FLAG_THRSAVED` is set in coroutines's `Operation.flags`.
- P: `FLAG_PENDING` is set in coroutines's `Operation.flags`.
- C: `FLAG_NOCANCEL` is set in coroutines's `Operation.flags`.

### Transitions

|  # | Trigger                         | Result                                  |
| --:|:------------------------------- |:--------------------------------------- |
|  1 | request op. called              | coroutine suspends calling request op.  |
|  2 | thread op. called               | coroutine suspends calling thread op.   |
|  3 | UV request callback             | request op. returns results             |
|  4 | coroutine resumed               | request op. returns as canceled         |
|  5 | request op. called              | coroutine suspends calling request op.  |
|  6 | coroutine yields or ends        | coroutine is unreferenced               |
|  7 | thread op. called               | coroutine suspends calling thread op.   |
|  8 | UV request callback             | coroutine is unreferenced               |
|  9 | operation called                | coroutine suspends calling operation    |
| 10 | UV request callback             | coroutine suspends again on request op. |
| 11 | coroutine resumed               | operation returns as canceled           |
| 12 | UV request callback             | coroutine suspends again on thread op.  |
| 13 | UV handle callback              | thread op. returns results              |
| 14 | coroutine resumed               | thread op. returns as canceled          |
| 15 | coroutine resumed               | thread op. returns as canceled          |
| 16 | same thread op. called          | coroutine suspends calling thread op.   |
| 17 | coroutine yields or ends        | coroutine is unreferenced               |
| 18 | other operation called          | coroutine suspends calling operation    |
| 19 | same canceled thread op. called | coroutine suspends calling thread op.   |
| 20 | UV handle callback              | thread op.'s handle starts closing      |
| 21 | other operation called          | coroutine suspends calling operation    |
| 22 | UV handle closed                | thread op.'s handle is released         |
| 23 | operation called                | coroutine suspends calling operation    |
| 24 | coroutine resumed               | operation returns as canceled           |
| 25 | UV request callback             | coroutine suspends again on operation   |
| 26 | coroutine resumed               | operation returns as canceled           |
| 27 | UV handle closed                | coroutine suspends again on request op. |
| 28 | UV handle closed                | coroutine suspends again on thread op.  |
_______________________
 1. `lcuT_resetreqopk`
	- `uv_<request>`
	- `startedopk`
 2. `lcuT_resetthropk`
	- `uv_<handle>_init`
	- `lcuT_armthrop`
	- `uv_<handle>_start`
	- `startedopk`
 3. `uv_<request>_cb`
	- `lcuU_endreqop`
	- `lcuU_resumereqop...`
		- `k_endop`
 4. `k_endop`
	- `cancelop`?
		- `uv_cancel`
 5. `lcuT_resetreqopk`
	- `uv_<request>`
	- `startedopk`
	- `...lcuU_resumereqop`
 6. `...lcuU_resumereqop`
 7. `lcuT_resetthropk`
	- `uv_<handle>_init`
	- `lcuT_armthrop`
	- `uv_<handle>_start`
	- `startedopk`
	- `...lcuU_resumereqop`
 8. `uv_<request>_cb`
	- `lcuU_endreqop`
 9. `lcuT_resetreqopk|lcuT_resetthropk`
	- `yieldresetk`
10. `uv_<request>_cb`
	- `lcuU_endreqop`
	- `lcuU_resumereqop...`
		- `k_resetopk`
			- `uv_<request>`
			- `startedopk`
11. `k_resetopk`
12. `uv_<request>_cb`
	- `lcuU_endreqop`
	- `lcuU_resumereqop...`
		- `k_resetopk`
			- `uv_<handle>_init`
			- `lcuT_armthrop`
			- `uv_<handle>_start`
			- `startedopk`
13. `uv_<handle>_cb`
	- `lcuU_endthrop`
	- `lcuU_resumethrop...`
		- `k_endop`
14. `k_endop`
15. `k_endop`
	- `cancelop`
		- `uv_close`
16. `lcuT_resetthropk`
	- `uv_<handle>_stop`?
	- `uv_<handle>_start`?
	- `startedopk`
	- `...lcuU_resumethrop`
17. `...lcuU_resumethrop`
	- `cancelop`
		- `uv_close`
18. `lcuT_resetreqopk|lcuT_resetthropk`
	- `checkreset`
		- `uv_close`
	- `yieldresetk`
	- `...lcuU_resumethrop`
19. `lcuT_resetthropk`
	- `uv_<handle>_stop`?
	- `uv_<handle>_start?`
	- `startedopk`
20. `uv_<handle>_cb`
	- `lcuU_endthrop`
		- `cancelop`
			- `uv_close`
21. `lcuT_resetreqopk|lcuT_resetthropk`
	- `yieldresetk`
22. `closedhdl`
23. `lcuT_resetreqopk|lcuT_resetthropk`
	- `yieldresetk`
24. `k_resetopk`
25. `uv_<handle>_cb`
	- `lcuU_endthrop`
	- !!!`cancelop`!!!
		- !!!`uv_close`!!!
26. `k_resetopk`
27. `closedhdl`
	- `lcuU_resumereqop...`
		- `k_resetopk`
			- `uv_<request>`
			- `startedopk`
28. `closedhdl`
	- `lcuU_resumereqop...`
		- `k_resetopk`
			- `uv_<handle>_init`
			- `lcuT_armthrop`
			- `uv_<handle>_start`
			- `startedopk`

Object
------

![Object States](objstates.svg)

### Transitions

| # | P | S | Action | Condition |
| - | - | - | ------ | --------- |
| 1 |   | T | `lcu_closeobj` | object closed or collected |
| 2 |   | T | `lcuT_awaitobj` | UV handle started |
| 3 |   | U | `closedobj` | UV handle closed |
| 4 |   | T | `lcuT_haltedobjop` | resumed by `coroutine.resume` |
| 5 |   | T | `lcu_closeobj` | object closed or collected |
| 6 |   | U | `lcuU_resumeobjop` | resumed by UV handle callback |
| 7 | 6 | U | `freethread` | coroutine suspended or terminated |
| 8 | 6 | T | `lcu_closeobj` | object closed or collected |
| 9 | 6 | T | `lcuT_awaitobj` | coroutine repeated operation |
_______________________
- P = Previous transition
- S = Call scope (T = Thread; U = UV loop)

Templates
=========

Request Operation
-----------------

```c
LCUI_FUNC void lcuM_addmyawaitf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"myawait", lua_myawait},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}

static int lua_myawait (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);  /* requires 'LCU_MODUPVS' upvalues */
	return lcuT_resetreqopk(L, sched, k_setupfunc, onreturn, cancancel);
}

static int k_setupfunc (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_myevent_t *myevent = (uv_myevent_t *)handle;
	/* check argments and obtain desired configs for myevent */
	/* leave on the stack values required to produce the results */
	int err = uv_myevent(loop, myevent, uv_onmyevent, /* configs */);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}

static int cancancel (lua_State *L) {
	/* we know the thread is not awaiting for this myevent anymore */
	/* inpect any global state that might need clean up */
	if (/* we still need 'uv_onmyevent' to be called for some clean up */)
		return 0;
	else
		return 1;
}

static void uv_onmyevent (uv_myevent_t *myrequest, /* myevent details */) {
	uv_loop_t *loop = myrequest->loop;
	uv_req_t *request = (uv_req_t *)myrequest;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		/* push values to yield to 'thread', for 'onreturn' to process */
		lcuU_resumereqop(loop, request, /* number of pushed values */);
	} else {
		/* request wasn't cancelled, we can do the clean up now */
	}
}

static int onreturn (lua_State *L) {
	/* use values left on the stack by 'k_setupfunc' and the ones yielded */
	/* by 'uv_onmyevent' to produce the values to be returned */
	return /* number of values to return from the top of the stack */;
}
```

Thread Operation
----------------

```c
LCUI_FUNC void lcuM_addmyawaitf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"myawait", lua_myawait},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}

static int lua_myawait (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);  /* requires 'LCU_MODUPVS' upvalues */
	return lcuT_resetthropk(L, UV_MYEVENT, sched, k_setupfunc, onreturn, cancancel);
}

static int k_setupfunc (lua_State *L, uv_handle_t *handle, uv_loop_t *loop) {
	uv_myevent_t *myevent = (uv_myevent_t *)handle;
	/* check argments and obtain desired configs for myevent */
	/* leave on the stack values required to produce the results */
	int err = 0;
	if (loop) err = lcuT_armthrop(L, uv_myevent_init(loop, myevent));
	else if (/* myevent is misconfigured? */) err = uv_myevent_stop(myevent);
	else return -1;  /* yield on success */
	if (err >= 0) err = uv_myevent_start(myevent, uv_onmyevent, /* configs */);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}

static int cancancel (lua_State *L) {
	/* we know the thread is not awaiting for this myevent anymore */
	/* inpect any global state that might need clean up */
	if (/* we still need 'uv_onmyevent' to be called for some clean up */)
		return 0;
	else
		return 1;
}

static void uv_onmyevent (uv_myevent_t *handle, /* myevent details */) {
	if (lcuU_endthrop(handle)) {
		lua_State *thread = (lua_State *)handle->data;
		/* push values to yield to 'thread', for 'onreturn' to process */
		lcuU_resumethrop((uv_handle_t *)handle, /* number of pushed values */);
	} else {
		/* 'cancancel' returned 0, and we can do the clean up now */
	}
}

static int onreturn (lua_State *L) {
	/* use values left on the stack by 'k_setupfunc' and the ones yielded */
	/* by 'uv_onmyevent' to produce the values to be returned */
	return /* number of values to return from the top of the stack */;
}
```

Object Operation
----------------

```c
#define MYOBJECT_CLASS	LCU_PREFIX"MyObject"

LCUI_FUNC void lcuM_addmyawaitf (lua_State *L) {
	static const luaL_Reg modf[] = {
		{"myobject", lua_myobject},
		{NULL, NULL}
	};
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}

LCUMOD_API int luaopen_coutil_myobject (lua_State *L) {
	static const luaL_Reg metf[] = {
		{"__gc", myobj_gc},
		{"__close", myobj_gc},
		{"close", myobj_close},
		{"await", myobj_await},
		{NULL, NULL}
	};
	luaL_newmetatable(L, MYOBJECT_CLASS);
	lcuL_setfuncs(L, metf, 0);
	return 1;
}

typedef struct MyObject {
	int flags;
	uv_myobject_t handle;
	/* any extra fields */
}

static int lua_myobject (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);  /* requires 'LCU_MODUPVS' upvalues */
	MyObject *myobj = newobject(L, MyObject, MYOBJECT_CLASS);
	/* check argments and obtain desired configs for 'myobj' */
	int err = uv_myobject_init(loop, lcu_toobjhdl(myobj), /* configs */);
	if (err < 0) return lcuL_pusherrres(L, err);
	/* initialize any extra fields */
	return 1;
}

static int myobj_gc (lua_State *L) {
	luaL_checkudata(L, 1, MYOBJECT_CLASS);
	lcu_closeobj(L, 1);
	return 0;
}

static int myobj_close (lua_State *L) {
	luaL_checkudata(L, 1, MYOBJECT_CLASS);
	lua_pushboolean(L, lcu_closeobj(L, 1));
	return 1;
}

static int myobj_await (lua_State *L) {
	lcu_Object *object = lcu_openedobj(L, 1, LCU_UDPSOCKETCLS);
	return lcuT_resetobjopk(L, object, startmyawait, stopmyawait, k_onreturn);
}

static int udpstartrecv (uv_handle_t *handle) {
	return uv_myobj_myevent_start((uv_myobject_t *)handle, uv_onmyevent);
}

static int udpstoprecv (uv_handle_t *handle) {
	return uv_myobj_myevent_stop((uv_myobject_t *)handle);
}

static void uv_onmyevent (uv_myobject_t *myobjhdl, /* myevent details */) {
	uv_handle_t *handle = (uv_handle_t *)myobjhdl;
	lua_State *thread = (lua_State *)handle->data;
	lcu_assert(thread);
	/* push values to yield to 'k_onreturn' */
	lcuU_resumeobjop(handle, /* number of pushed values */);
}

static int k_onreturn (lua_State *L, int status, lua_KContext ctx) {
	/* use values left on the stack by 'myobj_await' and the ones yielded */
	/* by 'uv_onmyevent' to produce the values to be returned */
	return /* number of values to return from the top of the stack */;
}
```
