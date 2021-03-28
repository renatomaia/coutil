Summary
=======

1. [Await Function](#await-function)
2. [Independent State](#independent-state)
3. [Coroutine Finalizers](#coroutine-finalizers)
4. [Events](#events)
5. [Queued Events](#queued-events)
6. [Mutex](#mutex)
7. [Promises](#promises)
8. [Channels](#channels)
9. [Thread Pools](#thread-pools)
10. [Preemptive Coroutines](#preemptive-coroutines)
11. [System Features](#system-features)
12. [System Information](#system-information)

Index
=====

- [`coutil.channel`](#channels)
	- [`channel.close`](#channelclose-ch)
	- [`channel.create`](#channelcreate-name)
	- [`channel.getnames`](#channelgetnames-names)
	- [`channel.sync`](#channelsync-ch-endpoint-)
- [`coutil.coroutine`](#preemptive-coroutines)
	- [`coroutine.close`](#coroutineclose-preemptco)
	- [`coroutine.load`](#coroutineload-chunk--chunkname--mode)
	- [`coroutine.loadfile`](#coroutineloadfile-filepath--mode)
	- [`coroutine.status`](#coroutinestatus-preemptco)
- [`coutil.event`](#events)
	- [`event.await`](#eventawait-e)
	- [`event.awaitall`](#eventawaitall-e1-)
	- [`event.awaitany`](#eventawaitany-e1-)
	- [`event.emitall`](#eventemitall-e-)
	- [`event.emitone`](#eventemitone-e-)
	- [`event.pending`](#eventpending-e)
- [`coutil.info`](#system-information)
	- [`info.getcpustats`](#infogetcpustats-)
		- [`cpuinfo:count`](#cpuinfocount-)
		- [`cpuinfo:stats`](#cpuinfostats-i-what)
	- [`info.getsystem`](#infogetsystem-what)
	- [`info.getprocess`](#infogetprocess-what)
- [`coutil.mutex`](#mutex)
	- [`mutex.islocked`](#mutexislocked-e)
	- [`mutex.lock`](#mutexlock-e)
	- [`mutex.ownlock`](#mutexownlock-e)
	- [`mutex.unlock`](#mutexunlock-e)
- [`coutil.promise`](#promises)
	- [`promise.awaitall`](#promiseawaitall-p-)
	- [`promise.awaitany`](#promiseawaitany-p-)
	- [`promise.create`](#promisecreate-)
	- [`promise.onlypending`](#promiseonlypending-p-)
	- [`promise.pickready`](#promisepickready-p-)
- [`coutil.queued`](#queued-events)
	- [`queued.await`](#queuedawait-e)
	- [`queued.awaitall`](#queuedawaitall-e1-)
	- [`queued.awaitany`](#queuedawaitany-e1-)
	- [`queued.emitall`](#queuedemitall-e-)
	- [`queued.emitone`](#queuedemitone-e-)
	- [`queued.isqueued`](#queuedisqueued-e)
	- [`queued.pending`](#queuedpending-e)
- [`coutil.spawn`](#coroutine-finalizers)
	- [`spawn.catch`](#spawncatch-h-f-)
	- [`spawn.trap`](#spawntrap-h-f-)
- [`coutil.system`](#system-features)
	- [`system.address`](#systemaddress-type--data--port--mode)
	- [`system.awaitch`](#systemawaitch-ch-endpoint-)
	- [`system.awaitsig`](#systemawaitsig-signal)
	- [`system.emitsig`](#systememitsig-pid-signal)
	- [`system.execute`](#systemexecute-cmd-)
	- [`system.file`](#systemfile-path--mode--uperm--gperm--operm)
		- [`file:close`](#fileclose-)
		- [`file:read`](#filereadbuffer--i--j--offset)
		- [`file:write`](#filewritedata--i--j--offset)
	- [`system.findaddr`](#systemfindaddr-name--service--mode)
		- [`addresses:close`](#addressesclose-)
		- [`addresses:getaddress`](#addressesgetaddress-address)
		- [`addresses:getdomain`](#addressesgetdomain-)
		- [`addresses:getsocktype`](#addressesgetsocktype-)
		- [`addresses:next`](#addressesnext-)
		- [`addresses:reset`](#addressesreset-)
	- [`system.halt`](#systemhalt-)
	- [`system.isrunning`](#systemisrunning-)
	- [`system.nameaddr`](#systemnameaddr-address--mode)
	- [`system.nanosecs`](#systemnanosecs-)
	- [`system.packenv`](#systempackenv-vars)
	- [`system.resume`](#systemresume-preemptco-)
	- [`system.run`](#systemrun-mode)
	- [`system.socket`](#systemsocket-type-domain)
		- [`socket:accept`](#socketaccept-)
		- [`socket:bind`](#socketbind-address)
		- [`socket:close`](#socketclose-)
		- [`socket:connect`](#socketconnect-address)
		- [`socket:getaddress`](#socketgetaddress-site--address)
		- [`socket:getdomain`](#socketgetdomain-)
		- [`socket:joingroup`](#socketjoingroup-multicast--interface)
		- [`socket:leavegroup`](#socketleavegroup-multicast--interface)
		- [`socket:listen`](#socketlisten-backlog)
		- [`socket:receive`](#socketreceive-buffer--i--j--address)
		- [`socket:send`](#socketsend-data--i--j--address)
		- [`socket:setoption`](#socketsetoption-name-value)
		- [`socket:shutdown`](#socketshutdown-)
	- [`system.suspend`](#systemsuspend-delay)
	- [`system.time`](#systemtime-update)
	- [`system.unpackenv`](#systemunpackenv-env--tab)
- [`coutil.threads`](#thread-pools)
	- [`threads.close`](#threadsclose-pool)
	- [`threads.count`](#threadscount-pool-options)
	- [`threads.create`](#threadscreate-size)
	- [`threads.dofile`](#threadsdofile-pool-filepath--mode-)
	- [`threads.dostring`](#threadsdostring-pool-chunk--chunkname--mode-)
	- [`threads.resize`](#threadsresize-pool-size--create)

Contents
========

Await Function
--------------

An _await function_ [suspends](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) the execution of the calling coroutine
(yields no value),
and also implies that the coroutine will be resumed on some specific condition.

Coroutines executing an _await function_ can be resumed explicitly by [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume).
In such case,
the _await function_ returns the values provided to the resume.
Otherwise,
the _await function_ returns as described in the following sections.
In any case,
the coroutine will not be implicitly resumed after the _await function_ returns.

Independent State
-----------------

Code chunks that run on a separate system thread are loaded into an [independent state](http://www.lua.org/manual/5.4/manual.html#lua_newstate) with only the [`package`](http://www.lua.org/manual/5.4/manual.html#6.3) library loaded.
This independent state inherits any [preloaded modules](http://www.lua.org/manual/5.4/manual.html#pdf-package.preload) from the caller of the function that creates the independent state.
Moreover,
all other [standard libraries](http://www.lua.org/manual/5.4/manual.html#6) are also provided as preloaded modules.
Therefore function [`require`](http://www.lua.org/manual/5.4/manual.html#pdf-require) can be called in the independent state to load all other standard libraries.
In particular, [basic functions](http://www.lua.org/manual/5.4/manual.html#6.1) can be loaded using `require "_G"`.

Just bare in mind that requiring the preloaded modules for the standard libraries does not set their corresponding global tables.
To mimic the set up of the [standard standalone interpreter](http://www.lua.org/manual/5.4/manual.html#7) use a code like below:

```lua
for _, module in ipairs{
	"_G",
	"package", -- loaded, but no global 'package'
	"coroutine",
	"table",
	"io",
	"os",
	"string",
	"math",
	"utf8",
	"debug",
} do
	_ENV[module] = require(module)
end
```

__Note__: These independent states run in a separate thread,
but share the same [memory allocation](http://www.lua.org/manual/5.4/manual.html#lua_Alloc) and [panic function](http://www.lua.org/manual/5.4/manual.html#lua_atpanic) of the caller.
Therefore, it is required that thread-safe implementations are used,
such as the ones used in the [standard standalone interpreter](http://www.lua.org/manual/5.4/manual.html#7).

Coroutine Finalizers
--------------------

Module `coutil.spawn` provides functions to execute functions in new coroutines with associated handler functions to deal with the results.

### `spawn.catch (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If any error is raised inside `f`,
the coroutine executes the error message handler function `h` with the error message as argument.
`h` is executed in the calling context of the raised error,
just like an error message handler in `xpcall`.
Returns the new coroutine.

### `spawn.trap (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If `f` executes without any error, the coroutine executes function `h` passing as arguments `true` followed by all the results from `f`.
In case of any error,
`h` is executed with arguments `false` and the error message.
In the latter case,
`h` is executed in the calling context of the raised error,
just like a error message handler in `xpcall`.
Returns the new coroutine.

Events
------

Module `coutil.event` provides functions for synchronization of coroutines using events.
Coroutines might suspend awaiting for events on any value (except `nil`) in order to be resumed when events are emitted on these values.

A coroutine awaiting an event on a value does not prevent the value nor the coroutine to be collected,
but the coroutine will not be collected as long as the value does not become garbage.

### `event.await (e)`

Equivalent to [`event.awaitany`](#eventawaitany-e1-)`(e)`.

### `event.awaitall ([e1, ...])`

[Await function](#await-function) that awaits an event on every one of values `e1, ...`.
Any `nil` in `e1, ...` is ignored.
Any repeated values in `e1, ...` are treated as a single one.
If `e1, ...` are not provided or are all `nil`, this function has no effect.

Returns `true` after events are emitted on all values `e1, ...`,
or if `e1, ...` are not provided or are all `nil`.

### `event.awaitany (e1, ...)`

[Await function](#await-function) that awaits an event on any of the values `e1, ...`.
Any `nil` in `e1, ...` is ignored.
At least one value in `e1, ...` must not be `nil`.

Returns the value in `e1, ...` on which the event was emmited,
followed by all the additional arguments passed to [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-).

### `event.emitall (e, ...)`

Resumes all coroutines awaiting for event `e` in the same order they were suspended.
The additional arguments `...` are passed to every resumed coroutine.
This function returns after resuming all coroutines awaiting event `e` at the moment its call is initiated.

Returns `true` if there was some coroutine awaiting the event,
or `false` otherwise.

### `event.emitone (e, ...)`

Same as [`event.emitall`](#eventemitall-e-),
but resumes only one single coroutine awaiting for event `e`,
if there is any.

### `event.pending (e)`

Returns `true` if there is some coroutine suspended awaiting for event `e`,
or `false` otherwise.

Queued Events
-------------

Module `coutil.queued` provides functions similar to module [`coutil.event`](#events),
however the functions in this module store events emitted in a queue to be consumed later,
as if they were emitted immediately after a coroutine awaits for them.

A coroutine awaiting an event on a value does not prevent the value nor the coroutine and the event arguments to be collected,
but the coroutine and the event arguments will not be collected as long as the value does not become garbage.

### `queued.await (e)`

Same as [`event.await`](#eventawait-e),
but it consumes a stored event emitted on value `e`,
if there is any.

### `queued.awaitall ([e1, ...])`

Same as [`event.awaitall`](#eventawaitall-e1-),
but it first attempts to consume one stored event on each value `e1, ...`,
and only await on the values `e1, ...` that do not have a stored event.

### `queued.awaitany (e1, ...)`

Same as [`event.awaitany`](#eventawaitany-e1-),
but if there is a stored event on any of values `e1, ...`,
the stored event on the leftmost value `e1, ...` is consumed instead of awaiting for events.

### `queued.emitall (e, ...)`

Same as [`event.emitall`](#eventemitall-e-),
but if there is no coroutine awaiting on `e`,
it stores the event to be consumed later.

### `queued.emitone (e, ...)`

Same as [`event.emitone`](#eventemitone-e-),
but if there is no coroutine awaiting on `e`,
it stores the event to be consumed later.

### `queued.isqueued (e)`

Returns `true` if there is some stored event on `e`,
or `false` otherwise.

### `queued.pending (e)`

Alias for [`event.pending`](#eventpending-e).

Mutex
-----

Module `coutil.mutex` provides functions for [mutual exclusion](https://en.wikipedia.org/wiki/Mutex) of coroutines when using a resource.

### `mutex.lock (e)`

Acquires to the current coroutine the exclusive ownership identified by value `e`.
If the ownership is not taken then the function acquires the ownership and returns immediately.
Otherwise,
it awaits an [event](#events) on value `e` until the ownership is released
(see [`mutex.unlock`](#mutexunlock-e)),
so it can be acquired to the coroutine.

### `mutex.unlock (e)`

Releases from the current coroutine the exclusive ownership identified by value `e`.
It also emits an [event](#events) on `e` to resume one of the coroutines awaiting to acquire this ownership
(see [`mutex.lock`](#mutexlock-e)).

### `mutex.islocked (e)`

Returns `true` if the exclusive ownership identified by value `e` is taken,
and `false` otherwise.

### `mutex.ownlock (e)`

Returns `true` if the exclusive ownership identified by value `e` belongs to the calling coroutine,
and `false` otherwise.

Promises
--------

Module `coutil.promise` provides functions for synchronization of coroutines using [promises](https://en.wikipedia.org/wiki/Futures_and_promises).
Promises are used to obtain results that will only be available at a later moment, when the promise is fulfilled.
Coroutines that claim an unfulfilled promise suspend awaiting its fulfillment in order to receive the results.
But once a promise is fulfilled its results become readily available for those that claims them.

### `promise.create ()`

Returns a new promise, followed by a fulfillment function.

The promise is a function that returns the promise's results.
If the promise is unfulfilled,
it suspends the calling coroutine awaiting its fulfillment.
If the promise is called with an argument that evaluates to `true`,
it never suspends the calling coroutine,
and just returns `true` if the promise is fulfiled,
or `false` otherwise.

The fulfillment function shall be called in order to fulfill the promise.
The arguments passed to the fulfillment function become the promise's results.
It returns `true` the first time it is called,
or `false` otherwise.

Coroutines suspeded awaiting a promise's fulfillment are actually suspended awaiting an event on the promise (see [`await`](#eventawait-e)).
Such coroutines can be resumed by emitting an event on the promise.
In such case,
the additional values to [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-) will be returned by the promise.
Similarly,
the coroutines can be resumed prematurely by [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume).
In such case,
the promisse returns the values provided to the resume
(like [`coroutine.yield`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield)).
In either case,
the promise will remain unfulfilled.
Therefore,
when the promise is called again,
it will suspend the calling coroutine awaiting an event on the promise
(the promise fulfillment).

The first time the fulfillment function is called,
an event is emitted on the promise with the promise's results as additional values.
Therefore,
to suspend awaiting the fulfillment of a promise,
a coroutine can simply await an event on the promise.
However,
once a promise is fulfilled,
any coroutine that suspends awaiting events on the promise will remain suspended until an event on the promise is emitted,
since the fulfillment function will not emit the event even if is called again.

### `promise.awaitall ([p, ...])`

[Await function](#await-function) that awaits the fulfillment of all promises `p, ...`.
Returns `true` when all promises `p, ...` are fulfilled.

### `promise.awaitany (p, ...)`

[Await function](#await-function) that awaits the fulfillment of any of the promises in `p, ...`,
if there are no fulfilled promises in `p, ...`.
Otherwise it returns immediately.
In any case, it returns a fulfilled promise.

### `promise.onlypending ([p, ...])`

Returns all promises `p, ...` that are unfulfilled.

### `promise.pickready ([p, ...])`

Returns the first promise `p, ...` that is fulfilled,
or no value if none of promises `p, ...` is fulfilled.

Channels
--------

Module `coutil.channel` provides functions for manipulation of _channels_ to be used to synchronize and copy values between [independent-states](#independent-state).

This library also sets a metatable for the _channels_,
where the `__index` field points to the table with all its functions.
Therefore, you can use the library functions in object-oriented style.
For instance, `channel.sync(ch, ...)` can be written as `ch:sync(...)`, where `ch` is a _channel_.

### `channel.close (ch)`

Closes channel `ch`.
Note that channels are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

In case of success,
this function returns `true`.
Otherwise it returns `false` plus an error message.

### `channel.create (name)`

Returns a new _channel_ with name `name`.

Channels with the same name share the same two opposite [_endpoints_](#systemawaitch-ch-endpoint-).

### `channel.getnames ([names])`

Returns a table mapping each name of existing channels to `true`.

If table `names` is provided,
returns `names`,
but first sets each of its string keys to either `true`,
if there is a channel with that name,
or `nil`.
In other words,
any non existent channel name as a key in `names` is removed from it.

### `channel.sync (ch, endpoint, ...)`

Similar to [`system.awaitch`](#systemawaitch-ch-endpoint-),
but does not await for a matching call.
In such case,
it returns `false` followed by message "empty".

Thread Pools
------------

Module `coutil.threads` provides functions for manipulation of _thread pools_ that execute code chunks loaded as _tasks_ using a set of distinct system threads.

This library also sets a metatable for the _thread pools_,
where the `__index` field points to the table with all its functions.
Therefore, you can use the library functions in object-oriented style.
For instance, `threads.dostring(pool, ...)` can be written as `pool:dostring(...)`, where `pool` is a _thread pool_.

### `threads.create ([size])`

Returns a new _thread pool_ with `size` system threads to execute its [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-).

If `size` is omitted,
returns a new reference to the _thread pool_ where the calling code is executing,
or `nil` if it is not executing in a _thread pool_ (_e.g._ the main process thread).

### `threads.resize (pool, size [, create])`

Defines that [_thread pool_](#threadscreate-size) `pool` shall keep `size` system threads to execute its [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-).

If `size` is smaller than the current number of threads,
the exceeding threads are destroyed at the rate they are released from the _tasks_ currently executing in `pool`.
Otherwise, new threads are created on demand until the defined value is reached.
Unless `create` evaluates to `true`,
in which case the new threads are created immediatelly.

### `threads.count (pool, options)`

Returns numbers corresponding to the ammount of components in [_thread pool_](#threadscreate-size) `pool` according to the following characters present in string `options`:

- `n`: the total number of [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-).
- `r`: the number of _tasks_ currently executing.
- `p`: the number of _tasks_ pending to be executed.
- `s`: the number of _tasks_ suspended on a [channel](#channelcreate-name).
- `e`: the expected number of system threads.
- `a`: the actual number of system threads.

### `threads.dostring (pool, chunk [, chunkname [, mode, ...]])`

Loads a chunk in an [independent state](#independent-state) as a new _task_ to be executed by the system threads from [_thread pool_](#threadscreate-size) `pool`.
It starts as soon as a system thread is available.

Arguments `chunk`, `chunkname`, `mode` are the same of [`load`](http://www.lua.org/manual/5.4/manual.html#pdf-load).
Arguments `...` are passed to the loaded chunk,
but only _nil_, _boolean_, _number_, _string_ and _light userdata_ values are allowed as such arguments.

Whenever the loaded `chunk` [yields](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) it reschedules itself as pending to be resumed,
and releases its running system thread.

Execution errors in the loaded `chunk` terminate the _task_,
and gerenate a [warning](http://www.lua.org/manual/5.4/manual.html#pdf-warn).

Returns `true` if `chunk` is loaded successfully.

__Note__: A _task_ can yield a channel name followed by an endpoint name and the other arguments of [`system.awaitch`](#systemawaitch-ch-endpoint-) to be suspended awaiting on a channel without the need to load other modules.

### `threads.dofile (pool, filepath [, mode, ...])`

Similar to [`threads:dostring`](#threadsdostring-pool-chunk--chunkname--mode-), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.4/manual.html#pdf-loadfile).

### `threads.close (pool)`

When this function is called from a _task_ of [_thread pool_](#threadscreate-size) `pool`
(_i.e._ using a reference obtained by calling [`system.threads()`](#threadscreate-size) with no arguments),
it has no effect other than prevent further use of `pool`.

Otherwise, it waits until there are either no more _tasks_ or no more system threads,
and closes `pool` releasing all of its underlying resources.

Note that when `pool` is garbage collected before this functions is called,
it will retain minimum resources until the termination of the Lua state they were created.
To avoid accumulative resource consumption by creation of multiple _thread pools_,
call this function on every _thread pool_.

Moreover, a _thread pool_ that is not closed will prevent the current Lua state to terminate (_i.e._ `lua_close` to return) until it has either no more tasks or no more system threads.

Preemptive Coroutines
---------------------

Module `coutil.coroutine` provides functions for manipulation of _preemptive coroutines_,
which execute a chunk in an [independent state](#independent-state) in a separate system thread (see [`system.resume`](#systemresume-preemptco-)).

This library also sets a metatable for the _preemtive coroutines_,
where the `__index` field points to the table with all its functions.
Therefore, you can use the library functions in object-oriented style.
For instance, `coroutine.resume(co, ...)` can be written as `co:resume(...)`, where `co` is a _preemtive coroutine_.

### `coroutine.close (preemptco)`

Similar to [`coroutine.close`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.close),
but for  [_prepemtive coroutines_](#coroutineload-chunk--chunkname--mode).

### `coroutine.load (chunk [, chunkname [, mode]])`

Returns a _preemptive coroutine_ with the code given by the arguments `chunk`, `chunkname`, `mode`,
which are the same arguments of [`load`](http://www.lua.org/manual/5.4/manual.html#pdf-load).

### `coroutine.loadfile ([filepath [, mode]])`

Similar to [`coroutine.load`](#coroutineload-chunk--chunkname--mode), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.4/manual.html#pdf-loadfile).

### `coroutine.status (preemptco)`

Similar to [`coroutine.status`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume),
but for [_prepemtive coroutines_](#coroutineload-chunk--chunkname--mode).

System Features
---------------

Module `coutil.system` provides functions that expose system functionalities,
including [await functions]("#await") to await on system conditions.

Unless otherwise stated,
all there functions return `false` plus an error message on failure,
and some truly value on success.

### `system.run ([mode])`

Resumes coroutines awaiting system conditions.

`mode` is a string that defines how `run` executes, as described below:

- `"loop"` (default): it executes continously resuming every awaiting coroutine when their system condition is satisfied,
until there are no more awaiting coroutines,
or [`system.halt`](#systemhalt-) is called.
- `"step"`: it resumes every ready coroutine once,
or waits to resume at least one coroutine that becomes ready.
- `"ready"`: it resumes only coroutines that are currently ready.

Returns `true` if there are remaining awaiting coroutines,
or `false` otherwise.

__Note__: when called with mode `"loop"` from the main thread of a [_task_](#threadsdostring-pool-chunk--chunkname--mode-) and there are only [`system.awaitch`](#systemawaitch-ch-endpoint-) calls pending, the task is suspended until one of the pending calls are resolved.

### `system.isrunning ()`

Returns `true` if [`system.run`](#systemrun-mode) is executing,
or `false` otherwise.

### `system.halt ()`

Causes [`system.run`](#systemrun-mode) to return prematurely after this function returns.

### `system.time ([update])`

Returns the last calculated timestamp used to evaluate time-related system events.
The timestamp is a number of seconds with precision of milliseconds.
It increases monotonically from some arbitrary point in time,
and is not subject to clock drift.

If `update` is provided and evaluates to `true`,
the cached timestamp is updated to the current time of the system before it is returned.

### `system.nanosecs ()`

Returns a timestamp in nanoseconds that represents the current time of the system.
It increases monotonically from some arbitrary point in time,
and is not subject to clock drift.

### `system.suspend ([delay])`

[Await function](#await-function) that awaits `delay` seconds since timestamp provided by [`system.time`](#systemtime-update).

If `delay` is not provided,
is `nil`,
or negative,
it is assumed as zero,
so the calling coroutine will be resumed as soon as possible.

Returns `true` in case of success.

### `system.emitsig (pid, signal)`

Emits signal indicated by `signal` to process with identifier `pid`.
Aside from the values for `signal` listed in [`system.awaitsig`](#systemawaitsig-signal),
this function also accepts the following values for `signal`:

| `signal` | POSIX | Action |
| -------- | ----- | ------ |
| `"STOP"` | SIGSTOP | stop |
| `"TERMINATE"` | SIGKILL | terminate |

`signal` can also be the platform-dependent number of the signal to be emitted.

### `system.awaitsig (signal)`

[Await function](#await-function) that awaits for the process to receive the signal indicated by string `signal`,
as listed below:

| `signal` | POSIX | Action | Indication |
| -------- | ----- | ------ | ---------- |
| `"abort"` | SIGABRT | core dump | Process shall **abort**. |
| `"brokenpipe"` | SIGPIPE | terminate | Write on a **pipe** with no one to read it. |
| `"child"` | SIGCHLD | ignore | **Child** process terminated, stopped, or continued. |
| `"clocktime"` | SIGALRM | terminate | Real or **clock time** elapsed. |
| `"continue"` | SIGCONT | continue | Process shall **continue**, if stopped. |
| `"cpulimit"` | SIGXCPU | core dump | Defined **CPU** time **limit** exceeded. |
| `"cputime"` | SIGVTALRM | terminate | **CPU** time used by the **process** elapsed. |
| `"cputotal"` | SIGPROF | terminate | **CPU** time used by the **process** and by the **system on behalf of the process** elapses. |
| `"filelimit"` | SIGXFSZ | core dump | Allowed **file** size **limit** exceeded. |
| `"hangup"` | SIGHUP | terminate | Terminal was closed. |
| `"interrupt"` | SIGINT | terminate | Terminal requests the process to terminate. (_e.g._ Ctrl+`C`) |
| `"polling"` | SIGPOLL | terminate | Event occurred on [watched file descriptor](https://pubs.opengroup.org/onlinepubs/9699919799/functions/ioctl.html). |
| `"quit"` | SIGQUIT | core dump | Terminal requests the process to **quit** with a [core dump](https://en.wikipedia.org/wiki/Core_dump). (_e.g._ Ctrl+`\`) |
| `"stdinoff"` | SIGTTIN | stop | **Read** from terminal while in **background**. |
| `"stdoutoff"` | SIGTTOU | stop | **Write** to terminal while in **background**. |
| `"stop"` | SIGTSTP | stop | Terminal requests the process to **stop**. (_e.g._ Ctrl+`Z`) |
| `"sysargerr"` | SIGSYS | core dump | **System** call with a **bad argument**. |
| `"terminate"` | SIGTERM | terminate | Process shall **terminate**. |
| `"trap"` | SIGTRAP | core dump | Exception or debug **trap** occurs. |
| `"urgentsock"` | SIGURG | core dump | **High-bandwidth data** is available at a **socket**. |
| `"userdef1"` | SIGUSR1 | terminate | **User-defined** conditions. |
| `"userdef2"` | SIGUSR2 | terminate | **User-defined** conditions. |
| `"winresize"` | SIGWINCH | ignore | Terminal **window size** has **changed**. |

Returns string `signal` in case of success.

### `system.packenv (vars)`

Returns a _packed environment_ that encapsulates environment variables from table `vars`,
which shall map variable names to the values they must assume.

__Note__: by indexing a _packed environment_,
like `env.LUA_PATH`,
a linear search is performed in the list of packed variables to find the value of variable `LUA_PATH`.
If such variable does not exist in the packed environment,
`nil` is returned.

### `system.unpackenv (env [, tab])`

Returns a table mapping all environment variable names in [packed environment](#systempackenv-vars) `env` to their corresponding values.
If table `tab` is provided,
no table is created,
and it is used to store the results returned.

### `system.execute (cmd, ...)`

[Await function](#await-function) that executes a new process,
and awaits its termination.
`cmd` is the path of the executable image for the new process.
Every other extra arguments are strings to be used as command-line arguments for the executable image of the new process.

Alternatively,
`cmd` can be a table with the fields described below.
Unless stated otherwise,
when one of these field are not defined in table `cmd`,
or `cmd` is a string,
the new process inherits the characteristics of the current process,
like the current directory,
the environment variables,
or standard files.

- `execfile`:
path of the executable image for the new process.
This field is required.

- `runpath`:
path of the current directory of the new process.

- `stdin`, `stdout`, `stderr`:
the standard input, output or error output of the new process.
The possible values are:
	- A [Lua file](http://www.lua.org/manual/5.4/manual.html#pdf-io.open) to be provided to the process.
	- A [stream socket](#systemsocket-type-domain) to be provided to the process.
	- `false` to indicate it should be discarded (_e.g._ `/dev/null` shall be used).
	- A string with the following characters that indicate a [stream socket](#systemsocket-type-domain) shall be created and stored in the field to allow communication with the process.
		- `r`: a readable stream socket to send data to the process.
		- `w`: a writable stream socket to receive data from the process.
		- `s`: a stream socket that allows transmission of stream sockets (see domain [`"share"`](#systemsocket-type-domain)).

- `arguments`:
table with the sequence of command-line arguments for the executable image of the new process.
When this field is not provided,
the new process's executable image receives no arguments.

- `environment`:
a packed environment created by [`system.packenv`](#systempackenv-vars),
or a table mapping environment variable names to the values they must assume for the new process.
In the latter case,
a packed environment is implicitly created from the table.
If this field is provided,
only the variables defined will be available for the new process.

If `cmd` is a table,
the field `pid` is set with the identifier of the new process
(see [`system.emitsig`](#systememitsig-pid-signal))
before the calling coroutine is suspended.

Returns the string `"exit"`,
followed by a number of the exit status
when the process terminates normally.
Otherwise,
it returns a string indicating the signal that terminated the program,
as listed in [`system.emitsig`](#systememitsig-pid-signal),
followed by the platform-dependent number of the signal.
For signals not listed there,
the string `"signal"` is returned instead.
Use the platform-dependent number to differentiate such signals.

### `system.resume (preemptco, ...)`

[Await function](#await-function) that is like [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume),
but executes the [_preemptive coroutine_](#coroutineload-chunk--chunkname--mode) `preemptco` on a separate system thread,
and awaits for its completion or [suspension](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield).
Moreover, only _nil_, _boolean_, _number_, _string_ and _light userdata_ values can be passed as arguments or returned from `preemptco`.

If the coroutine executing this [await function](#await-function) is explicitly resumed,
the execution of `preemptco` continues in the separate thread,
and it will not be able to be resumed again until it [suspends](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield).
In such case the results of the execution of `preemptco` are discarded.
Finally,
the explicitly resumed coroutine may not complete await functions until `preemptco` yields or terminates.

_Preemptive coroutines_ are executed using a limited set of threads that are also used by the underlying system.
The number of threads is given by environment variable [`UV_THREADPOOL_SIZE`](http://docs.libuv.org/en/v1.x/threadpool.html).

### `system.awaitch (ch, endpoint, ...)`

[Await function](#await-function) that awaits on an _endpoint_ of channel `ch` for a similar call on the opposite _endpoint_,
either from another coroutine, [_task_](#threadsdostring-pool-chunk--chunkname--mode-) or [_preemptive coroutine_](#coroutineload-chunk--chunkname--mode).

`endpoint` is either string `"in"` or `"out"`,
each identifying an opposite _endpoint_.
Therefore, the call `channel:await("in")` will await for a call like `channel:await("out")` on another channel with the same name.

Alternativelly,
if `endpoint` is either `nil` or `"any"`,
the call will await for a call on either _endpoints_.
For instance, the call `channel:await("any")` will match either a call `channel:await("in")` or `channel:await("out")`.

Returns `true` followed by the extra arguments `...` from the matching call.
Otherwise, return `false` followed by an error message related to obtaining the arguments from the matching call.
In any case,
if this call does not raise errors,
it resumed the coroutine, [_task_](#threadsdostring-pool-chunk--chunkname--mode-) or [_preemptive coroutine_](#coroutineload-chunk--chunkname--mode) of the matching call.

### `system.address (type [, data [, port [, mode]]])`

Returns a new IP address structure.
`type` is either the string `"ipv4"` or `"ipv6"`,
to indicate the address to be created shall be a IPv4 or IPv6 address,
respectively.

If `data` is not provided the structure created is initilized with null data:
`0.0.0.0:0` for `"ipv4"`,
or `[::]:0` for `"ipv6"`.
Otherwise,
`data` is a string with the information to be stored in the structure created.

If only `data` is provided,
it must be a literal address as formatted inside a URI,
like `"192.0.2.128:80"` (IPv4),
or `"[::ffff:c000:0280]:80"` (IPv6).
Moreover,
if `port` is provided,
`data` is a host address and `port` is a port number to be used to initialize the address structure.
The string `mode` controls whether `data` is text (literal) or binary.
It may be the string `"b"` (binary data),
or `"t"` (text data).
The default is `"t"`.

The returned object provides the following fields:

- `type`: (read-only) is either the string `"ipv4"` or `"ipv6"`,
to indicate the address is a IPv4 or IPv6 address,
respectively.
- `literal`: is the text (literal) representation of the address,
like `"192.0.2.128"` (IPv4) or `"::ffff:c000:0280"` (IPv6).
- `binary`: is the binary representation of the address,
like `"\192\0\2\128"` (IPv4) or `"\0\0\0\0\0\0\0\0\0\0\xff\xff\xc0\x00\x02\x80"` (IPv6).
- `port`: is the port number of the IPv4 and IPv6 address.

Moreover,
you can pass the object to the standard function `tostring` to obtain the address as a string inside a URI,
like `"192.0.2.128:80"` (IPv4) or `[::ffff:c000:0280]:80` (IPv6).

### `system.nameaddr (address [, mode])`

[Await function](#await-function) that searches for a network name for `address`,
and awaits for the names found.
If `address` is an address object,
it returns a host name and a port service name for the address.
If `address` is a number,
it returns the service name for that port number.
If `address` is a string,
it returns a canonical name for that network name.

The string `mode` can contain any of the following characters:

- `l`: for local names instead of _Fully Qualified Domain Names_ (FQDN).
- `d`: for names of datagram services instead of stream services.
- `i`: for names in the _Internationalized Domain Name_ (IDN) format.
- `u`: allows unassigned Unicode code points
(implies `i`).
- `a`: checks host name is conforming to STD3
(implies `i`).

By default,
`mode` is the empty string.

### `system.findaddr (name [, service [, mode]])`

[Await function](#await-function) that searches for the addresses of network name `name`,
and awaits for the addresses found.
If `name` is `nil`,
the loopback address is searched.
If `name` is `"*"`,
the wildcard address is searched.

`service` is either a service name to be used to resolve the port number of the resulting addresses,
or a port number to be set in the resulting addresses.
When `service` is absent,
the port zero is used in the results.
The string `mode` defines the search domain. 
It can contain any of the following characters:

- `4`: for IPv4 addresses.
- `6`: for IPv6 addresses.
- `m`: for IPv4-mapped addresses.
- `d`: for addresses for datagram sockets.
- `s`: for addresses for stream or passive sockets.

When neither `4` nor `6` are provided,
the search only includes addresses of the same type configured in the local machine.
When neither `d` nor `s` are provided,
the search behaves as if both `d` and `s` were provided.
By default,
`mode` is the empty string.

In case of success,
returns an _addresses_ object that encapsulates all addresses found,
and provides methods to navigate through the addresses found and get them.

### `addresses:close ()`

Closes `addresses`,
releasing all its internal resources.
It also prevents further use of its methods.

### `addresses:next ()`

Returns `false` if `addresses` points to its last address.
Otherwise,
returns `true` and makes `addresses` point to the next address.

### `addresses:reset ()`

Makes `addresses` point to the first address.

### `addresses:getaddress ([address])`

Stores the address pointed by `addresses` in [`address`](#systemaddress-type--data--port--mode),
and returns it.
If `address` is not provided,
the pointed address is stored in a new [address](#systemaddress-type--data--port--mode) and returned.

### `addresses:getdomain ()`

Returns the type of the address pointed by `addresses`.
The possible values are the same of attribute `type` of an [`address`](#systemaddress-type--data--port--mode).

### `addresses:getsocktype ()`

Returns the type of the socket to be used to connect to the address pointed by `addresses`.
The possible values are the same of argument `type` of [`system.socket`](#systemsocket-type-domain).

### `system.socket (type, domain)`

Creates a socket of the type specified by `type`,
which is either:

- `"datagram"` creates datagram socket for data transfers.
- `"stream"` creates stream socket for data transfers.
- `"passive"` creates passive socket to [accept](#socketaccept-) connected stream sockets.

The `domain` string defines the socket's address domain (or family),
which is either:

- `"ipv4"` for UDP (datagram) or TCP (stream) sockets over IPv4.
- `"ipv6"` for UDP (datagram) or TCP (stream) sockets over IPv6.
- `"local"` for sockets which addresses are file paths on Unix,
or pipe names on Windows.
- `"share"` same as `"local"`,
but allows for transmission of stream sockets.

In case of success,
it returns the new socket.

### `socket:close ()`

Closes socket `socket`.
Note that sockets are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

Returns `true` in case of success.

### `socket:getdomain ()`

Returns the address domain of `socket`,
The possible values are the same of argument `domain` in [`system.socket`](#systemsocket-type-domain).

### `socket:setoption (name, value)`

Sets `value` as the value of option `name` for socket `socket`.
This operation is not available for passive TCP sockets.
The available options are:

#### UDP Socket

- `"broadcast"`: `value` is `true` to enable broadcast in `socket`,
or `false` otherwise.
- `"mcastloop"`: `value` is `true` to enable loopback of outgoing multicast datagrams,
or `false` otherwise.
- `"mcastttl"`: `value` is a number from 1 to 255 to define the multicast time to live.
- `"mcastiface"`: `value` is the address of the interface for multicast,
or `nil` to define no interface.

#### TCP Socket

- `"keepalive"`: is a number of seconds of the initial delay of the periodic transmission of messages when the TCP keep-alive option is enabled,
or `nil` otherwise.
- `"nodelay"`: is `true` when coalescing of small segments shall be avoided,
or `false` otherwise.

#### Local Socket

- `"permission"`: is a string that indicate the permissions over the socket by processes run by other users.
It can contain the following characters:
	- `r` indicate processes have permission to read from the socket.
	- `w` indicate processes have permission to write to the socket.

Returns `true` in case of success.

### `socket:bind (address)`

Binds socket `socket` to the address provided as `address`.

For non-local sockets `address` must be an [IP address](#systemaddress-type--data--port--mode).
For local sockets `address` must be a string
(either a path on Unix or a pipe name on Windows).

Returns `true` in case of success.

### `socket:connect ([address])`

Binds socket `socket` to the peer address provided as `address`,
thus any data send over the socket is targeted to that `address`.

For non-local domain sockets,
`address` must be an [IP address](#systemaddress-type--data--port--mode).
For local domain sockets,
`address` must be a string
(either a path on Unix or a pipe name on Windows).

If `address` is not provided and `socket` is a datagram socket then it is unbinded from its previous binded peer address
(_i.e._ it is disconnected).
For stream sockets argument `address` is mandatory,
and it is necessary to bind the socket to a peer address before sending any data.
It is an [await function](#await-function) that awaits for the connection establishment on stream sockets.
This operation is not available for passive sockets.

Returns `true` in case of success.

### `socket:getaddress ([site [, address]])`

Returns the address associated with socket `socket`,
as indicated by `site`,
which can be:

- `"self"`: The socket's address (the default).
- `"peer"`: The socket's peer address.

For non-local domain sockets,
`address` can be an [IP address](#systemaddress-type--data--port--mode) to store the result,
otherwise a new object is returned with the result data.

### `socket:send (data [, i [, j [, address]]])`

[Await function](#await-function) that awaits until it sends through socket `socket` the substring of `data` that starts at `i` and continues until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

For unbinded datagram sockets `address` must be destination address,
but it must be omitted for datagram sockets binded to a peer address.
For stream sockets `address` is ignored,
except for `"share"` stream sockets,
where `address` might be the stream socket to be transferred.
This operation is not available for passive sockets.

Returns `true` in case of success.

__Note__: if `data` is a [memory](https://github.com/renatomaia/lua-memory),
it is not converted to a Lua string prior to have its specified contents transfered.

### `socket:receive (buffer [, i [, j [, address]]])`

[Await function](#await-function) that awaits until it receives from socket `socket` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

For datagram sockets,
if `address` is provided,
it is used to store the peer address that sent the data.
For stream sockets,
`address` is ignored.
This operation is not available for passive sockets.

In case of success,
this function returns the number of bytes copied to `buffer`.
For datagram sockets,
it also returns a boolean indicating whether the copied data was truncated.
For `"share"` stream sockets,
it might return a received stream socket after the number of bytes.

### `socket:joingroup (multicast [, interface])`

Joins network interface with address `interface` in the multicast group of address `multicast`,
thus datagrams targed to `multicast` address are delivered through address `interface`.
If `ifaceaddr` is not provided the socket `datagram` binded local address is used.

Both `multicast` and `interface` are string containing either textual (literal) or binary IP addresses
(see [`system.address`](#systemaddress-type--data--port--mode)).

This operation is only available for UDP sockets.

Returns `true` in case of success.

### `socket:leavegroup (multicast [, interface])`

Removes network interface with address `interface` from the multicast group of address `multicast`
(see [`socket:joingroup`](#socketjoingroup-multicast--interface)).

This operation is only available for UDP sockets.

Returns `true` in case of success.

### `socket:shutdown ()`

Shuts down the write side of stream socket `socket`.

This operation is only available for stream sockets.

Returns `true` in case of success.

### `socket:listen (backlog)`

Starts listening for new connections on passive socket `socket`.
`backlog` is a hint for the underlying system about the suggested number of outstanding connections that shall be kept in the socket's listen queue.

This operation is only available for passive sockets.

Returns `true` in case of success.

### `socket:accept ()`

[Await function](#await-function) that awaits until passive socket `socket` accepts a new connection.

This operation is only available for passive sockets.

In case of success,
this function returns a new stream socket for the accepted connection.

### `system.file (path [, mode [, uperm [, gperm [, operm]]]])`

Opens file from the path indicated by string `path`.
The string `mode` can contain the following characters that define properties of the file opened.

- `a`: write operations are done at the end of the file (implies `w`).
- `n`: if such file does not exist,
instead of returning an error,
a new file is created in `path` and opened.
- `N`: ensure a new file is created in `path`,
otherwise returns an error.
- `r`: allows reading operations.
- `s`: write operations transfers all data to hardware (implies `w`).
- `t`: file contents are erased,
that is,
it is truncated to length 0 (implies `w`).
- `w`: allows writing operations.
- `x`: file is not inheritable to child processes.

The remaining arguments are strings that indicate the file permissions for the current user (`uperm`),
users in the current group (`gperm`),
and other users (`operm`) to be used to create a new file when `n` or `N` are present in `mode`.
The following characters are allowed in these arguments to define the permissions:

- `r`: read permission.
- `w`: write permission.
- `x`: execute permission.

**Note**: these file permissions are not enforced by the call that creates the file.
They only affect future accesses.
Files created by this call always allows for all permissions regardless of the permissions defined.

When absent,
the default value for the arguments are:

- `mode` is `"r"`.
- `uperm` is `"rw"`.
- `gperm` is the value of `uperm`.
- `operm` is the value of `gperm`.

In case of success,
it returns the file opened.

### `file:close ()`

Closes file `file`.
Note that files are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

Returns `true` in case of success.

### `file:write(data [, i [, j [, offset]]])`

[Await function](#await-function) that awaits until it writes to file `file` the substring of `data` that starts at `i` and continues until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

If `offset` is provided,
the data is written from the `offset` provided from the begin of the file.
In such case,
the file offset is not changed by this call.
In the other case,
the current file offset is used and updated.

In case of success,
this function returns the number of bytes written to `file`.

### `file:read(buffer [, i [, j [, offset]]])`

[Await function](#await-function) that awaits until it reads from file `file` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

If `offset` is provided,
the file is read from the `offset` provided from the begin of the file.
In such case,
the file offset is not changed by this call.
In the other case,
the current file offset is used and updated.

In case of success,
this function returns the number of bytes copied to `buffer`.

System Information
------------------

Module `coutil.info` provides functions to get information about the system.

### `info.getprocess (what)`

Returns numbers corresponding to resource usage of the current process according to the following characters present in string `what`:

| Char | Unsupported | Description |
| ---- | ----------- | ----------- |
| `#`  |             | process identifier of the current process (number). |
| `^`  |             | process identifier of the parent process (number). |
| `m`  |             | resident memory size (bytes). |
| `U`  | Windows     | user CPU time used (seconds). |
| `S`  | Windows     | system CPU time used (seconds). |
| `T`  |             | maximum resident set size (bytes). |
| `M`  | Linux       | integral shared memory size (bytes). |
| `d`  | Linux       | integral unshared data size (bytes). |
| `=`  | Linux       | integral unshared stack size (bytes). |
| `p`  |             | page reclaims (soft page faults). |
| `P`  | Windows     | page faults (hard page faults). |
| `w`  | Linux       | swaps. |
| `i`  | Windows     | block input operations. |
| `o`  | Windows     | block output operations. |
| `>`  | Linux       | IPC messages sent. |
| `<`  | Linux       | IPC messages received. |
| `s`  | Linux       | signals received. |
| `x`  |             | voluntary context switches. |
| `X`  |             | involuntary context switches. |

### `info.getsystem (what)`

Returns numbers corresponding to resource usage of the system according to the following characters present in string `what`:

- `f`: amount of free memory available in the system, as reported by the kernel (bytes).
- `p`: total amount of physical memory in the system (bytes).
- `u`: current system uptime (seconds).
- `1`: system load (last 1 minute).
- `l`: system load (last 5 minutes).
- `L`: system load (last 15 minutes).

### `info.getcpustats ()`

Returns a `cpuinfo` object with statistics about the individual CPU of the system.

### `cpuinfo:count ()`

Returns the number of individual CPU information contained in `cpus`.
These CPU are identified by indexes from 1 to the number of individual CPU.

### `cpuinfo:stats (i, what)`

Returns numbers corresponding to usage statistics stored in `cpuinfo` for the CPU with index `i`, according to the following characters present in string `what`:

- `m`: CPU model name.
- `c`: current CPU clock speed (MHz).
- `u`: time the CPU spent executing normal processes in user mode (milliseconds).
- `n`: time the CPU spent executing prioritized (niced) processes in user mode (milliseconds).
- `s`: time the CPU spent executing processes in kernel mode (milliseconds).
- `i`: time the CPU was idle (milliseconds).
- `d`: time the CPU spent servicing device interrupts. (milliseconds).
