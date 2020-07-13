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
| `udp:send` | `UV_UDP_SEND` | `uv_udp_send_cb` |
| `system.findaddr` | `UV_GETADDRINFO` | `uv_getaddrinfo_cb` |
| `system.nameaddr` | `UV_GETNAMEINFO` | `uv_getnameinfo_cb` |
| --------- | ------- | -------- |
| `file:*` | `UV_FS` | `uv_fs_cb` |

### Thread Operations (`throp`)

Asynchronous operation perfomed over a UV handle that can be replicated for different threads resulting in the same effect.

| Operation | Handle | Callback |
| --------- | ------ | -------- |
| `system.suspend` | `UV_IDLE` | `uv_idle_cb` |
| `system.suspend` | `UV_TIMER` | `uv_timer_cb` |
| `system.awaitsig` | `UV_SIGNAL` | `uv_signal_cb` |
| `system.execute` | `UV_PROCESS` | `uv_exit_cb` |
| --------- | ------ | -------- |
| `system.awaitpath` | `UV_FS_EVENT` | `uv_fs_event_cb` |
| `system.awaitpath` | `UV_FS_POLL` | `uv_fs_poll_cb` |

### Object Operations (`objop`)

Asynchronous operation perfomed over a UV handle associated with a system resource with is own identity that cannot be replicated for each thread (_e.g._ socket, pipe or TTY file).

| Operation | Handle | Callback |
| --------- | ------ | -------- |
| `udp:receive` | `UV_UDP` | `uv_udp_recv_cb` |
| `tcp:receive` | `UV_TCP` | `uv_read_cb` |
| `tcp:accept` | `UV_TCP` | `uv_connection_cb` |
| `pipe:receive` | `UV_NAMED_PIPE` | `uv_read_cb` |
| `pipe:accept` | `UV_NAMED_PIPE` | `uv_connection_cb` |
| --------- | ------ | -------- |
| `tty:receive` | `UV_TTY` | `uv_read_cb` |
| `tty:accept` | `UV_TTY` | `uv_connection_cb` |

Identifiers
-----------

| Short | Long | Description |
| ----- | ---- | ----------- |
| `loop` | `loop` | [UV loop](http://docs.libuv.org/en/v1.x/loop.html) |
| `hdl` | `handle` | [UV handle](http://docs.libuv.org/en/v1.x/handle.html) |
| `req` | `request` | [UV request](http://docs.libuv.org/en/v1.x/request.html) |
| `thr` | `thread` | [Lua coroutine](http://www.lua.org/manual/5.3/manual.html#2.6) |
| `kctx` | `kcontext` | [Lua continuation context](http://www.lua.org/manual/5.3/manual.html#lua_KContext) |
| `kfn` | `kfunction` | [Lua continuation function](http://www.lua.org/manual/5.3/manual.html#lua_KFunction) |
| `idx` | `index` | [Lua stack index](http://www.lua.org/manual/5.3/manual.html#4.3) |
| `arg` | `argument` | [Lua function argument position](http://www.lua.org/manual/5.3/manual.html#lua_CFunction) |
| `obj` | `object` | System resource representation (_e.g._ socket, file, process) |
| `flg` | `flags` | [Bit field](https://en.wikipedia.org/wiki/Bit_field) |
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

Modules
=======

Internal
--------

### `lcuconf.h`

Definitions to customize implementation in similar fashion to `luaconf.h` of Lua.

| Name | Type | Description |
| ---- | ---- | ----------- |
| LCUI_FUNC | function prefix | prefix for internal functions. |
| LCULIB_API | function prefix | prefix for API functions. |
| LCUMOD_API | function prefix | prefix for `luaopen_` functions. |
| LCU_PREFIX | string | prefix for metatables. |
| LCU_PIPEADDRBUF | number | max. length of [pipe names](manual.md#socketgetaddress-site--address) (Unix path or Windows pipe names). |
| LCU_EXECARGCOUNT | number | max. number of arguments for a [new process](manual.md#systemexecute-cmd-). |
| LCU_NETHOSTNAMESZ | number | max. length of [resolved host names](manual.md#systemnameaddr-address--mode). |
| LCU_NETSERVNAMESZ | number | max. length of [resolved service names](manual.md#systemnameaddr-address--mode). |
| lcu_assert assert | function macro | function for internal assertions. |

### `lmodaux.c`:

Auxiliary functions for implementation of Lua modules.

### `loperaux.c`: 

Auxiliary functions for implementation of module operations,
specially [await functions](manual.md#await).

API
---

### `lsyslib.c`: 

C API for manipulation of CoUtil Lua objects.

### `lsysmod.c`: 

Lua module of CoUtil.

States
======

Thread
------

![Thread States](thrstates.svg)

### Transitions

|  # |  P | S | Action | Condition |
| -- | -- | - | ------ | --------- |
|  1 |    | T | `lcuT_resetreqopk` | UV request started |
|  1 |  7 | T | `resetop` | UV request reused |
|  1 |  8 | T | `resetop` | UV handle used as UV request |
|  2 |    | T | `lcuT_resetthropk+lcuT_armthrop` | UV handle initialized |
|  2 |  7 | T | `resetop` | UV request used as UV handle |
|  2 |  8 | T | `resetop` | UV handle reused |
|  3 |    | U | `lcuU_endreqop` | resumed by UV request callback |
|  4 |    | T | `endop` | resumed by `coroutine.resume` |
|  5 |  2 | T | `cancelop` | thread operation setup failed |
|  5 | 10 | T | `endop+cancelop` | resumed by `coroutine.resume` |
|  6 |    | U | `lcuU_resumethrop` | resumed by UV handle callback |
|  7 |    | U | `endop` | UV request concluded |
|  8 |    | U | `closedhdl` | UV handle closed |
|  9 |  6 | U | `cancelop` | coroutine suspended or terminated |
|  9 |  6 | T | `lcuT_resetthropk+uv_close` | different UV handle is active |
| 10 |  6 | T | `lcuT_resetthropk` | coroutine repeats operation |
_______________________
- P = Previous transition
- S = Call scope (T = thread; U = UV loop)

### Resources

- operation:
```c
/* from Lua stack (inside module closure) */
lcu_Operation *operation = tothrop(L);
/* from UV handle */
lcu_Operation *operation = (lcu_Operation *)handle;
/* from UV request */
lcu_Operation *operation = (lcu_Operation *)request;
```
- handle/request:
```c
if (lcuL_maskflag(op, FLAG_REQUEST)) {
	uv_req_t *request = torequest(operation);
} else {
	uv_handle_t *handle = tohandle(operation);
}
```
- coroutine:
```c
lua_State *thread = (lua_State *)( lcuL_maskflag(op, FLAG_REQUEST) ?
                                   torequest(op)->data :
                                   tohandle(op)->data );
```

### Conditions

- Freed:
```c
/* coroutine notification pending */
lcu_assert(lcuL_maskflag(operation, FLAG_REQUEST));
lcu_assert(!lcuL_maskflag(operation, FLAG_PENDING));
/* no armed operation */
uv_req_t *request = torequest(operation);
lcu_assert(request->type == UV_UNKNOWN_REQ);
/* coroutine is subject to garbage collection */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TNIL);
```
- Armed:
```c
/* coroutine notification pending */
lcu_assert(!lcuL_maskflag(operation, FLAG_REQUEST));
lcu_assert(lcuL_maskflag(operation, FLAG_PENDING));
/* armed thread operation */
uv_handle_t *handle = tohandle(operation);
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
lcu_assert(lcuL_maskflag(operation, FLAG_PENDING));
if (lcuL_maskflag(op, FLAG_REQUEST)) {
	/* armed request operation */
	uv_req_t *request = torequest(operation);
	lcu_assert(request->type != UV_UNKNOWN_REQ);
} else {
	/* armed thread operation */
	uv_handle_t *handle = tohandle(operation);
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
if (lcuL_maskflag(op, FLAG_REQUEST)) {
	/* armed request operation */
	uv_req_t *request = torequest(operation);
	lcu_assert(request->type != UV_UNKNOWN_REQ);
} else {
	/* thread operation close pending */
	uv_handle_t *handle = tohandle(operation);
	lcu_assert(handle->type != UV_UNKNOWN_HANDLE);
	lcu_assert(uv_is_closing(handle));
}
/* LCU_COREGISTRY[&operation] == coroutine */
lua_pushlightuserdata(L, (void *)operation);
lcu_assert(lua_gettable(L, LCU_COREGISTRY) == LUA_TTHREAD);
lcu_assert(lua_tothread(L, -1) == thread);
```

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
if (lcuL_maskflag(object, LCU_OBJFLAG_CLOSED))
	lcu_clearflag(object, LCU_OBJFLAG_CLOSED);
else
	lcu_setflag(object, LCU_OBJFLAG_CLOSED);
```

### Conditions

- Ready:
```c
/* not being closed */
lcu_assert(!lcuL_maskflag(object, LCU_OBJFLAG_CLOSED));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* object is subject to garbage collection */
```
- Await:
```c
/* not being closed */
lcu_assert(!lcuL_maskflag(object, LCU_OBJFLAG_CLOSED));
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
lcu_assert(!lcuL_maskflag(object, LCU_OBJFLAG_CLOSED));
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
lcu_assert(lcuL_maskflag(object, LCU_OBJFLAG_CLOSED));
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
lcu_assert(lcuL_maskflag(object, LCU_OBJFLAG_CLOSED));
/* no awaiting coroutine */
lcu_assert(handle->data == NULL);
/* object is subject to garbage collection */
```
