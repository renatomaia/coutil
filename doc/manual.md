Summary
=======

- [Basic Concepts](#basic-concepts)
	- [Await Function](#await-function)
	- [Blocking Mode](#blocking-mode)
	- [Independent State](#independent-state)
	- [Transferable Values](#transferable-values)
	- [Failures](#failures)
	- [Object-Oriented Style](#object-oriented-style)
- [Multithreading](#multithreading)
	- [Coroutine Finalizers](#coroutine-finalizers)
	- [State Coroutines](#state-coroutines)
	- [Thread Pools](#thread-pools)
- [Synchronization](#synchronization)
	- [Channels](#channels)
	- [Events](#events)
	- [Queued Events](#queued-events)
	- [Mutex](#mutex)
	- [Promises](#promises)
- [System Features](#system-features)
	- [Event Processing](#event-processing)
	- [Thread Synchronization](#thread-synchronization)
	- [Time Measure](#time-measure)
	- [System Processes](#system-processes)
	- [Network & IPC](#network--ipc)
	- [File System](#file-system)
	- [Standard Streams](#standard-streams)
	- [System Information](#system-information)
- [Index](#index)

---

Basic Concepts
==============

This section describes some basic concepts pertaining the modules described in the following sections.

Await Function
--------------

An _await function_ [suspends](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) the execution of the calling coroutine
(yields no value),
but also registers the coroutine to be resumed implicitly on some specific condition.

Coroutines executing an _await function_ can be resumed explicitly by [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume).
In such case,
the _await function_ returns the values provided to `coroutine.resume`.
Otherwise,
the _await function_ returns as described in the following sections.
In any case,
the coroutine is not registered to be implicitly resumed anymore once the _await function_ returns.

Blocking Mode
-------------

Some [await functions](#await-function) accept as argument a string with character `~` to avoid the function to yield.
In such case,
the function works like ordinary functions blocking the entire Lua state execution until its completion,
thus preventing any other coroutine to execute.

Independent State
-----------------

[Chunks](http://www.lua.org/manual/5.4/manual.html#3.3.2) that run on a separate system thread are loaded into a [independent state](http://www.lua.org/manual/5.4/manual.html#lua_newstate) with only the [`package`](http://www.lua.org/manual/5.4/manual.html#6.3) library loaded.
This _independent state_ inherits any [preloaded modules](http://www.lua.org/manual/5.4/manual.html#pdf-package.preload) from the caller of the function that creates it.
Moreover,
all other [standard libraries](http://www.lua.org/manual/5.4/manual.html#6) are also provided as preloaded modules.
Therefore function [`require`](http://www.lua.org/manual/5.4/manual.html#pdf-require) can be called in the _independent state_ to load all other standard libraries.
In particular, [basic functions](http://www.lua.org/manual/5.4/manual.html#6.1) can be loaded using `require "_G"`.

Just bare in mind that requiring the preloaded modules for the standard libraries does not set their corresponding global tables.
To mimic the set up of the [standard standalone interpreter](http://www.lua.org/manual/5.4/manual.html#7) do something like:

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

**Note**: these _independent states_ run in a separate thread,
but share the same [memory allocation](http://www.lua.org/manual/5.4/manual.html#lua_Alloc) and [panic function](http://www.lua.org/manual/5.4/manual.html#lua_atpanic) of the caller.
Therefore, it is required that thread-safe implementations are used,
such as the ones used in the [standard standalone interpreter](http://www.lua.org/manual/5.4/manual.html#7).

Transferable Values
-------------------

Values that are transfered between [independent states](#independent-state) are copied or recreated in the target state.
Only _nil_, _boolean_, _number_, _string_ and _light userdata_ values are allowed as _transferable values_.
_Strings_, in particular, are replicated in every state they are transfered to.

Failures
--------

Unless otherwise stated,
functions return [fail](http://www.lua.org/manual/5.4/manual.html#6) on failure,
plus an error message as a second result and a system-dependent error code as a third result.
On success,
some non-false value is returned.

Object-Oriented Style
---------------------

Some modules that create objects also set a metatable for these objects,
where the `__index` field points to the table with all the functions of the module.
Therefore, you can use these library functions in object-oriented style on the objects they create.

Multithreading
==============

This section describes modules for creation of threads of execution of Lua code,
either using standard coroutines,
or other abstractions that allows to execute code on multiple system threads.

Coroutine Finalizers
--------------------

Module `coutil.spawn` provides functions to execute functions in new coroutines with an associated handler function to deal with the results.

### `spawn.catch (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If any error is raised in `f`,
the coroutine executes the error message handler function `h` with the error message as argument.
`h` is executed in the calling context of the raised error,
just like an error message handler in `xpcall`.
Returns the new coroutine.

### `spawn.trap (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If `f` executes without any error, the coroutine executes function `h` passing as arguments `true` followed by all the results from `f`.
In case of any error in `f`,
`h` is executed with arguments `false` and the error message.
In the latter case,
`h` is executed in the calling context of the raised error,
just like a error message handler in `xpcall`.
Returns the new coroutine.

State Coroutines
----------------

Module `coutil.coroutine` provides functions similar to the ones on module [`coroutine`](http://www.lua.org/manual/5.4/manual.html#6.2),
but for _state coroutines_.
In contrast to standard _thread coroutines_ that execute a function in a [Lua thread](http://www.lua.org/manual/5.4/manual.html#lua_newthread),
_state coroutines_ execute a chunk in an [independent state](#independent-state) (see [`system.resume`](#systemresume-co-)).

You can access these library functions on _state coroutines_ in [object-oriented style](#object-oriented-style).
For instance, `coroutine.status(co, ...)` can be written as `co:status()`, where `co` is a _state coroutine_.

### `coroutine.close (co)`

Similar to [`coroutine.close`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.close),
but for _state coroutines_.

### `coroutine.load (chunk [, chunkname [, mode]])`

On success,
returns a new _state coroutine_ loaded with the code given by the arguments `chunk`, `chunkname`, `mode`,
which are the same arguments of [`load`](http://www.lua.org/manual/5.4/manual.html#pdf-load).

### `coroutine.loadfile ([filepath [, mode]])`

Similar to [`coroutine.load`](#coroutineload-chunk--chunkname--mode), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.4/manual.html#pdf-loadfile).

### `coroutine.status (co)`

Similar to [`coroutine.status`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.status),
but for _state coroutines_.
In particular,
it returns:

- `"running"`: if the _state coroutine_ is running
(that is, it is being executed in another system thread);
- `"suspended"`: if the _state coroutine_ is suspended in a call to yield
(like, when its chunk calls [`coroutine.yield`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield)),
or if it has not started running yet;
- `"dead"`: if the coroutine has finished its chunk,
or if it has stopped with an error.

Thread Pools
------------

Module `coutil.threads` provides functions for manipulation of [_thread pools_](#threadscreate-size) that execute code chunks loaded as [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-) using a set of distinct system threads.

You can access these library functions on _thread pools_ in [object-oriented style](#object-oriented-style).
For instance, `threads.dostring(pool, ...)` can be written as `pool:dostring(...)`, where `pool` is a _thread pool_.

### `threads.create ([size])`

On success,
returns a new _thread pool_ with `size` system threads to execute its [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-).

If `size` is omitted,
returns a new reference to the _thread pool_ where the calling code is executing,
or `nil` if it is not executing in a _thread pool_
(for instance, the main process thread).

### `threads.resize (pool, size [, create])`

Defines that [_thread pool_](#threadscreate-size) `pool` shall keep `size` system threads to execute its [_tasks_](#threadsdostring-pool-chunk--chunkname--mode-).

If `size` is smaller than the current number of threads,
the exceeding threads are destroyed at the rate they are released from the _tasks_ currently executing in `pool`.
Otherwise, new threads are created on demand until the defined value is reached.
However,
if `create` evaluates to `true`,
new threads are created to reach the defined value before the function returns.

Returns `true` on success.

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
Arguments `...` are [transferable values](#transferable-values) passed to the loaded chunk.

Whenever the loaded `chunk` [yields](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) it reschedules itself as pending to be resumed,
and releases its running system thread.

Execution errors in the loaded `chunk` terminate the _task_,
and gerenate a [warning](http://www.lua.org/manual/5.4/manual.html#pdf-warn).

Returns `true` if `chunk` is loaded successfully.

**Note**: the loaded `chunk` can [yield](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) a string with a channel name followed by an endpoint name and the other arguments of [`system.awaitch`](#systemawaitch-ch-endpoint-) to suspend the _task_ awaiting on a channel without the need to load other modules.
In such case,
[`coroutine.yield`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.yield) returns just like [`system.awaitch`](#systemawaitch-ch-endpoint-).

### `threads.dofile (pool, filepath [, mode, ...])`

Similar to [`threads:dostring`](#threadsdostring-pool-chunk--chunkname--mode-), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.4/manual.html#pdf-loadfile).

### `threads.close (pool)`

When this function is called from a [_task_](#threadsdostring-pool-chunk--chunkname--mode-) of [_thread pool_](#threadscreate-size) `pool`
(that is, using a reference obtained by calling [`thread.create()`](#threadscreate-size) without any argument),
it has no effect other than prevent further use of `pool`.

Otherwise, it waits until there are either no more _tasks_ or no more system threads,
and closes `pool` releasing all of its underlying resources.

Note that when `pool` is garbage collected before this function is called,
it will retain minimum resources until the termination of the Lua state it was created.
To avoid accumulative resource consumption by creation of multiple _thread pools_,
call this function on every _thread pool_.

Moreover, a _thread pool_ that is not closed will prevent the current Lua state to terminate (`lua_close` to return) until it has either no more tasks or no more system threads.

Returns `true` on success.

Synchronization
===============

This section describes modules for synchronization and communication between threads of execution of Lua code,
either between standard coroutines,
or [independent states](#independent-state) running in separate system threads.

Channels
--------

Module `coutil.channel` provides functions for manipulation of _channels_ to be used to synchronize and transfer values between coroutines running in different [states](#independent-state).

_Channels_ can also be used by coroutines in the same _state_.
However [events](#events) are usually more flexible and efficient in such case.

You can access these library functions on _channels_ in [object-oriented style](#object-oriented-style).
For instance, `channel.sync(ch, ...)` can be written as `ch:sync(...)`, where `ch` is a _channel_.

### `channel.close (ch)`

Closes channel `ch`.
Note that channels are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

### `channel.create (name)`

In case of success,
returns a new _channel_ with name given by string `name`.

Channels with the same name share the same two opposite [_endpoints_](#systemawaitch-ch-endpoint-).

### `channel.getname (ch)`

Returns the name of channel `ch`.

### `channel.getnames ([names])`

In case of success,
returns a table mapping each name of existing channels to `true`.

If table `names` is provided,
it checks only the names stored as string keys in `names`,
and returns `names` with each of its string keys set to either `true`,
when there is a channel with that name,
or `nil` otherwise.
In other words,
any non existent channel name as a key in `names` is removed from it.

### `channel.sync (ch, endpoint, ...)`

Similar to [`system.awaitch`](#systemawaitch-ch-endpoint-) when there already is a matching call on the endpoint of channel `ch`.
Otherwise,
it [fails](#failures) with message `"empty"`.

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
If `e1, ...` are not provided or are all `nil`, this function returns immediately.

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

System Features
===============

Module `coutil.system` provides functions that expose system functionalities,
including [await functions]("#await-function") to await on system conditions.

Event Processing
----------------

This section describes functions of `coutil.system` related to the processing of system events and resumption of coroutines executing [await functions]("#await-function") of `coutil.system`.

### `system.run ([mode])`

Resumes coroutines executing [await functions]("#await-function") of `coutil.system` when they are _ready_,
which is when their _await function_ have some result to process.
Note that even though a coroutine is _ready_,
its _await function_ may not return just yet,
because it may require additional results to conclude and return.

`mode` is a string that defines how `run` executes,
as described below:

- `"loop"` (default): it executes continously resuming every awaiting coroutine when they become _ready_,
until there are no more coroutines awaiting,
or [`system.halt`](#systemhalt-) is called.
- `"step"`: it resumes every _ready_ coroutine once,
or waits to resume at least one coroutine that becomes _ready_.
- `"ready"`: does not wait,
and just resumes coroutines that are currently _ready_.

Returns `true` if there are remaining awaiting coroutines,
or `false` otherwise.

**Note**: when called with `mode` as `"loop"` in the chunk of a [_task_](#threadsdostring-pool-chunk--chunkname--mode-) and there are only [`system.awaitch`](#systemawaitch-ch-endpoint-) calls pending,
this call yields,
suspending the task until one of the pending calls is matched.

### `system.isrunning ()`

Returns `true` if [`system.run`](#systemrun-mode) is executing,
or `false` otherwise.

### `system.halt ()`

Sets up [`system.run`](#systemrun-mode) to terminate prematurely,
and returns before `system.run` terminates.
Must be called while `system.run` is executing.

Thread Synchronization
----------------------

This section describes functions of `coutil.system` for thread synchronization and communication using [_channels_](#channelcreate-name) and [_state coroutines_](#state-coroutines).

### `system.awaitch (ch, endpoint, ...)`

[Await function](#await-function) that awaits on an _endpoint_ of [channel](#channelcreate-name) `ch` for a similar call on the opposite _endpoint_,
either from another coroutine,
or [_task_](#threadsdostring-pool-chunk--chunkname--mode-).

`endpoint` is either string `"in"` or `"out"`,
each identifying an opposite _endpoint_.
Therefore, the call `system.awaitch(ch1, "in")` will await for a call like `system.awaitch(ch2, "out")` if both `ch1` and `ch2` are distinct channels with the same name.

Alternativelly,
if `endpoint` is either `nil` or `"any"`,
the call will await for a matching call on either _endpoints_.
For instance, the call `system.awaitch(ch1, "any")` will match either a call `system.awaitch(ch2, "in")`,
or `system.awaitch(ch2, "out")`,
or even `system.awaitch(ch2, "any")` if both `ch1` and `ch2` are distinct channels with the same name.

Returns `true` followed by the extra [transferable](#transferable-values) arguments `...` from the matching call.
Otherwise,
it [fails](#failures) with an error message related to transfering the arguments from the matching call.
In any case,
if this call does not raise errors,
nor is resumed prematurely by a call of [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume),
then it successfully resumed the coroutine or _task_ of the matching call.

### `system.resume (co, ...)`

Similar to [`coroutine.resume`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.resume),
but for [_state coroutines_](#state-coroutines).
It is an [await function](#await-function) that executes _state coroutine_ `co` on a separate system thread,
and awaits for its completion or suspension.
Moreover,
only [transferable values](#transferable-values) can be passed as arguments, yielded, or returned from `co`.

If the coroutine executing this _await function_ is explicitly resumed,
the execution of `co` continues in the separate thread,
and it will not be able to be resumed again until it suspends.
In such case,
the results of this execution of `co` are discarded.

_State coroutines_ are executed using a limited set of threads that are also used by the underlying system to execute some _await functions_.
The number of threads is given by environment variable [`UV_THREADPOOL_SIZE`](http://docs.libuv.org/en/v1.x/threadpool.html).
Therefore,
_state coroutines_ that do not execute briefly,
might degrade the performance of some _await functions_.

For long running tasks,
consider using a [_thread pool_](#thread-pools).

Time Measure
------------

This section describes functions of `coutil.system` to obtain a measure of time or to wait for a particular period of time.

### `system.nanosecs ()`

Returns a timestamp in nanoseconds that represents the current time of the system.
It increases monotonically from some arbitrary point in time,
and is not subject to clock drift.

### `system.time ([mode])`

Returns a timestamp as a number of seconds with precision of milliseconds according to the value of `mode`,
as described below:

- `"cached"` (default): the cached timestamp periodically updated by [`system.run`](#systemrun-mode) before resuming coroutines that are ready.
It is used as the current time to calculate when future calls to [system.suspend](#systemsuspend-seconds--mode) shall be resumed.
It increases monotonically from some arbitrary point in time,
and is not subject to clock drift.
- `"updated"`: updates the cached timestamp to reflect the current time,
and returns this updated timestamp.
- `"epoch"`: a timestamp relative to [UNIX Epoch](https://en.wikipedia.org/wiki/Unix_time),
based on the current time set in the system.
Therefore,
unlike the other options,
it is affected by discontinuous jumps in the system time
(for instance, if the system administrator manually changes the system time).

### `system.suspend ([seconds [, mode]])`

[Await function](#await-function) that awaits `seconds` since timestamp provided by [`system.time("cached")`](#systemtime-mode).

If `seconds` is not provided,
is `nil`,
or negative,
it is assumed as zero,
so the calling coroutine will be resumed as soon as possible.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

System Processes
----------------

This section describes functions of `coutil.system` for manipulation of the current process
(signals, priority, current directory, and environment variables),
and other processes,
as well as creating new processes by executing programs.

### `system.emitsig (pid, signal)`

Emits a signal specified by string `signal` to the process with identifier `pid`.
Aside from the values for `signal` listed in [`system.awaitsig`](#systemawaitsig-signal),
this function also accepts the following values for `signal`:

| `signal` | POSIX | Action |
| -------- | ----- | ------ |
| `"STOP"` | SIGSTOP | stop |
| `"TERMINATE"` | SIGKILL | terminate |

`signal` can also be the platform-dependent number of the signal to be emitted.

### `system.awaitsig (signal)`

[Await function](#await-function) that awaits for the current process to receive the signal indicated by string `signal`,
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
| `"interrupt"` | SIGINT | terminate | Terminal requests the process to terminate. (Ctrl+`C`) |
| `"polling"` | SIGPOLL | terminate | Event occurred on [watched file descriptor](https://pubs.opengroup.org/onlinepubs/9699919799/functions/ioctl.html). |
| `"quit"` | SIGQUIT | core dump | Terminal requests the process to **quit** with a [core dump](https://en.wikipedia.org/wiki/Core_dump). (Ctrl+`\`) |
| `"stdinoff"` | SIGTTIN | stop | **Read** from terminal while in **background**. |
| `"stdoutoff"` | SIGTTOU | stop | **Write** to terminal while in **background**. |
| `"stop"` | SIGTSTP | stop | Terminal requests the process to **stop**. (Ctrl+`Z`) |
| `"sysargerr"` | SIGSYS | core dump | **System** call with a **bad argument**. |
| `"terminate"` | SIGTERM | terminate | Process shall **terminate**. |
| `"trap"` | SIGTRAP | core dump | Exception or debug **trap** occurs. |
| `"urgentsock"` | SIGURG | core dump | **High-bandwidth data** is available at a **socket**. |
| `"userdef1"` | SIGUSR1 | terminate | **User-defined** conditions. |
| `"userdef2"` | SIGUSR2 | terminate | **User-defined** conditions. |
| `"winresize"` | SIGWINCH | ignore | Terminal **window size** has **changed**. |

Returns string `signal` in case of success.

**Note**: not all signals are availabe in all platforms.
For more details,
see the documentation on [signal support by libuv](http://docs.libuv.org/en/v1.x/signal.html).

### `system.getpriority (pid)`

On success,
returns a string and a number corresponding to the scheduling priority of the process with identifier `pid`.
The first value returned is one of the following strings,
which indicates some convenient distinct priority value (from highest to lowest):

- `"highest"`
- `"high"`
- `"above"`
- `"normal"`
- `"below"`
- `"low"`

The string `"other"` is returned for any other priorities values placed inbetween these reference values.

The second returned value is a number ranging from -20 (for highest priority) to 19 (for lowest priority) corresponding to the scheduling priority of the process.

### `system.setpriority (pid, value)`

Changes the scheduling priority of the process with identifier `pid`.
`value` can be any of the string or number values described as the return values of [`system.getpriority`](#systemgetpriority-pid),
except string `"other"`,
which does not denote a specific priority value.

### `system.getdir ()`

On success,
returns the path of the current working directory.

### `system.setdir (path)`

Changes the current working directory to the path in string `path`.

### `system.getenv ([name])`

If `name` is not provided,
returns a table mapping all current environment variable names to their corresponding values.

Alternatively,
If `name` is a table,
the results are added to it,
and `name` is returned.

Otherwise,
`name` is the name of the process environment variable which value must be returned.
[Fails](#failures) if the variable `name` is not defined.

### `system.setenv (name, value)`

Sets the contents of string `value` as the value of the process environment variable `name`.
If `value` is `nil`,
deletes the environment variable.

### `system.packenv (vars)`

Returns a _packed environment_ that encapsulates environment variables from table `vars`,
which shall map variable names to their corresponding value.

**Note**: by indexing a _packed environment_ `env`,
like `env.LUA_PATH`,
a linear search is performed in the list of packed variables to return the value of variable `LUA_PATH`.
If such variable does not exist in the packed environment,
`nil` is returned.

### `system.unpackenv (env [, tab])`

Returns a table mapping all environment variable names in [packed environment](#systempackenv-vars) `env` to their corresponding values.
If table `tab` is provided,
the results are added to it,
and `tab` is returned.

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
the standard input, output and error output of the new process.
The possible values are:
	- A [Lua file](http://www.lua.org/manual/5.4/manual.html#pdf-io.open) to be provided to the process.
	- A [stream socket](#systemsocket-type-domain) to be provided to the process.
	- `false` to indicate it should be discarded
	(for instance, `/dev/null` shall be used).
	- A string with the following characters that indicate a [stream socket](#systemsocket-type-domain) shall be created and stored in the field to allow communication with the process.
		- `r`: a readable stream socket to receive data from the process.
		- `w`: a writable stream socket to send data to the process.
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
before the calling coroutine is suspended.

Returns the string `"exit"`,
followed by a number of the exit status
when the process terminates normally.
Otherwise,
it returns a string indicating the signal that terminated the program,
as accepted by [`system.emitsig`](#systememitsig-pid-signal),
followed by the platform-dependent number of the signal.
For signals not listed there,
the string `"signal"` is returned instead.
Use the platform-dependent number to differentiate such signals.
It [fails](#failures) if the process cannot be created.

Network & IPC
-------------

This section describes functions of `coutil.system` for creation of network sockets and other [IPC](https://en.wikipedia.org/wiki/Inter-process_communication) mechanisms.

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
The string `mode` controls whether `data` is textual or binary.
It may be the string `"b"` (binary data),
or `"t"` (text data).
The default is `"t"`.

The following calls create addresses with the same contents:

```lua
system.address("ipv4")
system.address("ipv4", "0.0.0.0:0")
system.address("ipv4", "0.0.0.0", 0)
system.address("ipv4", "\0\0\0\0", 0, "b")
```

The returned object provides the following fields:

- `type`: (read-only) is either the string `"ipv4"` or `"ipv6"`,
to indicate the address is a IPv4 or IPv6 address,
respectively.
- `literal`: is the text representation of the host address,
like `"192.0.2.128"` (IPv4) or `"::ffff:c000:0280"` (IPv6).
- `binary`: is the binary representation of the host address,
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
returns an `addresses` object that encapsulates all addresses found,
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
Returns no value.

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
but allows for [transmission of stream sockets](#socketwrite-data--i--j--address).

In case of success,
it returns the new socket.

### `socket:close ()`

Closes socket `socket`.
Note that sockets are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

### `socket:getdomain ()`

Returns the address domain of `socket`,
The possible values are the same of argument `domain` in [`system.socket`](#systemsocket-type-domain).

### `socket:setoption (name, value, ...)`

Sets `value` as the value of option `name` for socket `socket`.
This operation is not available for passive TCP sockets.
The available options are:

#### UDP Socket

- `"broadcast"`: `value` is `true` to enable broadcast in `socket`,
or `false` otherwise.
- `"mcastloop"`: `value` is `true` to enable loopback of outgoing multicast datagrams,
or `false` otherwise.
- `"mcastttl"`: `value` is a number from 1 to 255 to define the multicast time to live.
- `"mcastiface"`: `value` is the _literal host address_ of the interface for multicast.
Otherwise,
an appropriate interface is chosen by the system.
- `"mcastjoin"` or `"mcastleave"`: `value` is the _literal host address_ of the multicast group the application wants to **join** or **leave**.
If an extra third argument is provided,
it is the _literal host address_ of the local interface with which the system should **join** or **leave** the multicast group.
Otherwise,
an appropriate interface is chosen by the system.
If an extra fourth argument is provided,
it defines the _literal host address_ of a source the application shall receive data from.
Otherwise,
it can receive data from any source.

#### TCP Socket

- `"keepalive"`: `value` is a number of seconds of the initial delay of the periodic transmission of messages when the TCP keep-alive option is enabled,
or `nil` otherwise.
- `"nodelay"`: `value` is `true` when coalescing of small segments shall be avoided,
or `false` otherwise.

#### Local Socket

- `"permission"`: `value` is a string that indicate the permissions over the socket by processes run by other users.
It can contain the following characters:
	- `r` indicate processes have permission to read from the socket.
	- `w` indicate processes have permission to write to the socket.

### `socket:bind (address)`

Binds socket `socket` to the address provided as `address`.

For non-local sockets `address` must be an [IP address](#systemaddress-type--data--port--mode).
For local sockets `address` must be a string
(either a path on Unix or a pipe name on Windows).

### `socket:connect ([address])`

Binds socket `socket` to the peer address provided as `address`,
thus any data send over the socket is targeted to that `address`.

For non-local domain sockets,
`address` must be an [IP address](#systemaddress-type--data--port--mode).
For local domain sockets,
`address` must be a string
(either a path on Unix or a pipe name on Windows).

If `address` is not provided and `socket` is a datagram socket then it is unbinded from its previous binded peer address
(that is, it is disconnected).
For stream sockets argument `address` is mandatory,
and it is necessary to bind the socket to a peer address before sending any data.
It is an [await function](#await-function) that awaits for the connection establishment on stream sockets.
This operation is not available for passive sockets.

### `socket:getaddress ([site [, address]])`

On success,
returns the address associated with socket `socket`,
as indicated by `site`,
which can be:

- `"self"`: The socket's address (the default).
- `"peer"`: The socket's peer address.

For non-local domain sockets,
`address` can be an [IP address](#systemaddress-type--data--port--mode) to store the result,
otherwise a new address object is returned with the result data.
For local domain sockets,
it returns a string
(either a path on Unix or a pipe name on Windows).

### `socket:write (data [, i [, j [, address]]])`

[Await function](#await-function) that awaits until it sends through socket `socket` the substring of `data` that starts at `i` and continues until `j`,
following the same sematics of these arguments in functions of [memory](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#lua-module).

For unbinded datagram sockets,
`address` must be destination address,
but it must be omitted for datagram sockets binded to a peer address.
For stream sockets,
`address` is ignored,
except for `"share"` stream sockets,
where `address` might be the stream socket to be transferred.
This operation is not available for passive sockets.

**Note**: if `data` is a [memory](https://github.com/renatomaia/lua-memory),
it is not converted to a Lua string prior to have its specified contents transfered.

### `socket:read (buffer [, i [, j [, address]]])`

[Await function](#await-function) that awaits until it receives from socket `socket` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`,
following the same sematics of these arguments in functions of [memory](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#lua-module).

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
it returns the transfered stream socket after the number of bytes.

### `socket:shutdown ()`

Shuts down the write side of stream socket `socket`.

This operation is only available for stream sockets.

### `socket:listen (backlog)`

Starts listening for new connections on passive socket `socket`.
`backlog` is a hint for the underlying system about the suggested number of outstanding connections that shall be kept in the socket's listen queue.

This operation is only available for passive sockets.

### `socket:accept ()`

[Await function](#await-function) that awaits until passive socket `socket` accepts a new connection.

This operation is only available for passive sockets.

In case of success,
this function returns a new stream socket for the accepted connection.

File System
-----------

This section describes functions of `coutil.system` to access files and the file system.

### `system.linkfile (path, destiny [, mode])`

[Await function](#await-function) that awaits for the creation of a link on path `destiny` refering the file on path given by string `path`.

It [fails](#failures) if a file in `destiny` already exists.

String `mode` might contain `s` to create a symbolic link,
instead of a hard link.

On Windows,
`mode` can contain the following characters to control how the symbolic link will be created.
(all imply `s`):

- `d`: indicates that path points to a directory.
- `j`: request that the symlink is created using junction points.

`mode` might also contain character `~` to execute it in [blocking mode](#blocking-mode).

### `system.copyfile (path, destiny [, mode])`

[Await function](#await-function) that awaits until file from `path` is copied to path given by string `destiny`.

`mode` can contain the following characters:

- `~`: executes in [blocking mode](#blocking-mode).
- `n`: fails if already exists a file in `destiny`.
- `c`: attempt to create a copy-on-write reflink.
- `C`: creates a copy-on-write reflink,
or fails otherwise.

### `system.movefile (path, destiny [, mode])`

[Await function](#await-function) that awaits until file from `path` is moved to path given by string `destiny`.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `system.removefile (path [, mode])`

[Await function](#await-function) that awaits to remove from path `path` a file,
or an empty directory if `mode` is a string with character `d`.
`mode` might also contain character `~` to execute it in [blocking mode](#blocking-mode).

### `system.maketemp (prefix [, mode])`

[Await function](#await-function) that awaits for the creation of a uniquely named temporary directory or file with the prefix given by string `prefix`.

String `mode` might contain any of the following characters to make it create a file instead of a directory.
These characters also define the sequence of values returned by the call in case of success.

- `f`: returns the path to the **file** created.
- `o`: returns the created file already **opened**.

If `mode` does not contain any of the above characters,
this function returns the path to the created directory on success.

`mode` might also be prefixed with character `~` to execute it in [blocking mode](#blocking-mode).

By default,
mode is the empty string.

### `system.makedir (path, perm [, mode])`

[Await function](#await-function) that awaits the creation of a new directory on `path`.
`perm` indicates the permissions of the directory to be created,
just like argument `perm` of [`file:grant`](#filegrant-perm--mode).

It [fails](#failures) if a file in `path` already exists.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `system.listdir (path [, mode])`

[Await function](#await-function) that awaits to obtain the list of the file entries in the directory in `path`.

On success,
returns an [iterator](http://www.lua.org/manual/5.4/manual.html#3.3.5) that lists the entries obtained.
The _control variable_ is the name of the file entry,
and an additional _loop variable_ is a string with the type of the file entry
(same values by `?` in [`file:info`](#fileinfo-mode)).

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

This function never [fails](#failures).
It raises errors instead.

### `system.touchfile (path [, mode, times...])`

Similar to [`file:touch`](#filetouch-mode-times),
but for file in path given by string `path`.
The other arguments are the same of [`file:touch`](#filetouch-mode-times).

`mode` can also be prefixed with `l` to indicate that,
if `path` refers a symbolic link,
the time values are changed for the symbolic link file instead of the link's target file.

### `system.ownfile (path, uid, gid [, mode])`

Similar to [`file:own`](#fileown-uid-gid--mode),
but for file in path given by string `path`.
The other arguments are the same of [`file:own`](#fileown-uid-gid--mode).

`mode` can also be prefixed with `l` to indicate that,
if `path` refers a symbolic link,
the ownership is changed for the symbolic link file instead of the link's target file.

### `system.grantfile (path, perm [, mode])`

Similar to [`file:grant`](#filegrant-perm--mode),
but for file in path given by string `path`.
The other arguments are the same of [`file:grant`](#filegrant-perm--mode).

### `system.openfile (path [, mode [, perm]])`

[Await function](#await-function) that awaits until file in `path` in opened.
The string `mode` can contain the following characters that define how the file is opened.

- `a`: write operations are done at the end of the file (implies `w`).
- `n`: if such file does not exist,
instead of [failing](#failures),
a new file is created in `path` and opened.
- `N`: ensure a new file is created in `path`,
or [fails](#failures) otherwise.
- `r`: allows reading operations.
- `f`: write operations [transfers all data to the file's device](#fileflush-mode) (implies `w`).
- `t`: file contents are erased,
that is,
it is truncated to length 0 (implies `w`).
- `w`: allows writing operations.
- `x`: file is not inheritable to child processes (see [`O_CLOEXEC`](https://www.man7.org/linux/man-pages/man2/open.2.html)).

`mode` might also be prefixed with character `~` to execute it in [blocking mode](#blocking-mode).

When either `n` or `N` are present in `mode`,
`perm` indicates the permissions of the file to be created,
just like argument `perm` of [`file:grant`](#filegrant-perm--mode).

**Note**: these file permissions are not enforced by the call that creates the file.
They only affect future accesses.
Files created by this call always allows for all permissions regardless of the permissions defined.

On success,
returns the file opened.

### `file:close ([mode])`

[Await function](#await-function) that awaits until `file` is closed.
Note that files are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `file:read (buffer [, i [, j [, offset [, mode]]]])`

[Await function](#await-function) that awaits until it reads from file `file` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`,
following the same sematics of these arguments in functions of [memory](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#lua-module).

If `offset` is provided,
it is the byte offset from the begin of the file where the data must be read.
In such case,
the file offset is not changed by this call.
Otherwise,
the current file offset is used and updated.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

On success,
returns the number of bytes copied to `buffer`.

### `file:write (data [, i [, j [, offset [, mode]]]])`

[Await function](#await-function) that awaits until it writes to file `file` the substring of `data` that starts at `i` and continues until `j`,
following the same sematics of these arguments in functions of [memory](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#lua-module).

If `offset` is provided,
it is the byte offset from the begin of the file where the data must be written.
In such case,
the file offset is not changed by this call.
Otherwise,
the current file offset is used and updated.

If `data` is a file opened for reading,
it works as if the contents of the file were the string `data`.
In such case,
`i` and `j` are mandatory,
and must be positive.
Moreover,
`offset` is ignored in this case,
thus the current offset of `file` is used and updated.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

On success,
returns the number of bytes written to `file`.

### `file:resize (size [, mode])`

[Await function](#await-function) that awaits until it resizes `file` to `size` bytes.
If the file is larger than this size, the extra data is lost.
If the file is shorter,
it is extended,
and the extended part reads as null bytes.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `file:flush ([mode])`

[Await function](#await-function) that awaits until it saves any data written to `file` into the file's device.

If string `mode` contains `d`,
it minimizes the device activity by skipping metadata not necessary for later data retrieval.

`mode` might also contain character `~` to execute it in [blocking mode](#blocking-mode).

### `file:touch ([mode, times...])`

[Await function](#await-function) that awaits until it changes the access and modification times of `file`.
String `mode` contains a sequence of characters indicating how each provided time value `times...` shall be used in `file`.

- `a`: as the access time.
- `m`: as the modification time.
- `b`: as both access and modification times.

Whenever a time value is not provided for either the access or modification time,
the current time set in the system is used.

`mode` might also be prefixed with character `~` to execute it in [blocking mode](#blocking-mode).
Unlike other characters,
`~` does not consume a time value from `times...`.

**Note**: all time values are seconds relative to [UNIX Epoch](https://en.wikipedia.org/wiki/Unix_time) with sub-second precision.

### `file:own (uid, gid [, mode])`

[Await function](#await-function) that awaits until it changes the owner IDs of `file` to numbers `uid` for the user ID,
and `gid` for the group ID.
If the `uid` or `gid` is specified as -1,
then that ID is not changed.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `file:grant (perm [, mode])`

[Await function](#await-function) that awaits until it changes the permission bits of `file`.
`perm` must be either a number with the [permission bits](#permissions),
or a string with characters defining the bits to be set for the file,
as listed below:

- `U` Set-**user**-ID _bit_.
- `G` Set-**group**-ID _bit_.
- `S` **Sticky** bit.
- `r` **Read** permission for the file's onwer _user_ ID.
- `w` **Write** permission for the file's onwer _user_ ID.
- `x` **Execute** permission for the file's onwer _user_ ID.
- `R` **Read** permission for the file's owner _group_ ID.
- `W` **Write** permission for the file's owner _group_ ID.
- `X` **Execute** permission for the file's owner _group_ ID.
- `4` **Read** permission for _others_.
- `2` **Write** permission for _others_.
- `1` **Execute** permission for _others_.

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

### `file:info (mode)`

[Await function](#await-function) that awaits for information related to `file`.

On success,
returns values according to the following characters in string `mode`:

| Character | Value | Description |
| --------- | ----- | ----------- |
| `?` | string <!-- st_mode --> | **Type** of the file as one of the field names of bitmasks for [file types](#file-types). |
| `M` | integer <!-- st_mode --> | [_Bit flags_](#systemfilebits) of the file (_imode_ **mode**). |
| `_` | integer <!-- st_flags --> | **User defined** flags for the file. |
| `u` | integer <!-- st_uid --> | Owner **user** _identifier_ (uid). |
| `g` | integer <!-- st_gid --> | Owner user **group** identifier (gid). |
| `#` | integer <!-- st_ino --> | ID of the file in the file system (_inode_ **number**). |
| `D` | integer <!-- st_dev --> | ID of the **device** _containing_ the file. |
| `d` | integer <!-- st_rdev --> | ID of the **device** _represented_ by the file, or `0` if not applicable. |
| `*` | integer <!-- st_nlink --> | _Number_ of **hard links** to the file. |
| `B` | integer <!-- st_size --> | Total size of the file (**bytes**). |
| `b` | integer <!-- st_blocks --> | Number of 512B **blocks** allocated for the file. |
| `i` | integer <!-- st_blksize --> | **Ideal** transfer _block size_ for the file. |
| `v` | integer <!-- st_gen --> | **Generation** number of the file. |
| `a` | number <!-- st_atim --> | Time of last **access** of the file. |
| `s` | number <!-- st_ctim --> | Time of last **status** change of the file. |
| `m` | number <!-- st_mtim --> | Time of last **modification** of the file. |
| `c` | number <!-- st_birthtim --> | Time of file **creation**. |

`mode` can also contain any of the characters valid for argument `perm` of [`file:grant`](#filegrant-perm--mode).
For these characters a boolean is returned indicating whether such bit is set.

`mode` might also be prefixed with character `~` to execute it in [blocking mode](#blocking-mode).
Unlike other characters,
`~` does not produce a value to be returned.

**Note**: all time values returned are seconds relative to [UNIX Epoch](https://en.wikipedia.org/wiki/Unix_time).

### `system.filebits`

Table with the following fields with numberic values of the [bitmask](https://en.wikipedia.org/wiki/Mask_(computing)) for [file mode bits](https://man7.org/linux/man-pages/man7/inode.7.html).

#### File Types

- `type`: Bitmask for the following fields identifying different file types:
	- `socket`: Socket file type.
	- `link`: Symbolic link file type.
	- `file`: Regular file type.
	- `block`: Block device file type.
	- `directory`: Directory file type.
	- `character`: Character device file type.
	- `pipe`: FIFO file type.

#### Permissions

- `setuid`: Set-user-ID bit.
- `setgid`: Set-group-ID bit.
- `sticky`: Sticky bit.
- `ruser`: Read permission bit for the file's onwer user ID.
- `wuser`: Write permission bit for the file's onwer user ID.
- `xuser`: Execute permission bit for the file's onwer user ID.
- `rgroup`: Read permission bit for the file's owner group ID.
- `wgroup`: Write permission bit for the file's owner group ID.
- `xgroup`: Execute permission bit for the file's owner group ID.
- `rother`: Read permission bit for others.
- `wother`: Write permission bit for others.
- `xother`: Execute permission bit for others.

### `system.fileinfo (path, mode)`

Similar to [`file:info`](#fileinfo-mode)
but for file in path given by string `path`.
In addition to the characters supported by [`file:info`](#fileinfo-mode),
`mode` can also contain:

| Character | Value | Description |
| --------- | ----- | ----------- |
| `p` | string <!-- uv_fs_realpath --> | Canonicalized absolute **path** name for the file. |
| `=` | string <!-- uv_fs_readlink --> | The **value** of the symbolic link, or `nil` if `path` does not refer a symbolic link. |
| `@` | string <!-- f_type --> | _Type_ of the **file system** of the file. |
| `N` | integer <!-- f_type --> | **Numeric** ID of the _type_ of the file system of the file. |
| `I` | integer <!-- f_bsize --> | **Ideal** transfer _block size_ for the file system of the file. |
| `A` | integer <!-- f_bavail --> | Free blocks **available** to unprivileged user in the file system of the file. |
| `F` | integer <!-- f_bfree --> | **Free** _blocks_ in the file system of the file. |
| `f` | integer <!-- f_ffree --> | **Free** _inodes_ in the file system of the file. |
| `T` | integer <!-- f_blocks --> | **Total** data _blocks_ in the file system of the file. |
| `t` | integer <!-- f_files --> | **Total** _inodes_ in the file system of the file. |

Moreover,
`mode` can also be prefixed with `l` to indicate that,
if `path` refers a symbolic link,
the values corresponding to characters supported by [`file:info`](#fileinfo-mode) shall be about the symbolic link file instead of the link's target file.
The value of the other options are not affected.
Similar to `~`,
`l` does not produce a value to be returned.

Standard Streams
----------------

This section describes objects provided in `coutil.system` to access the current process standard streams.

### `system.stdin|stdout|stderr`

Terminal, [socket](#systemsocket-type-domain) or [file](#systemopenfile-path--mode--perm) representing the standard input (`stdin`), output (`stdout`), or error (`stderr`) stream of the current process.
Or `nil` if the type of such stream is unsupported.

### `terminal:close ()`

Closes terminal `terminal`.

### `terminal:read (buffer [, i [, j]])`

Same as [read](#socketread-buffer--i--j--address) of stream sockets.

### `terminal:write (data [, i [, j]])`

Same as [write](#socketwrite-data--i--j--address) of stream sockets.

### `terminal:shutdown ()`

Same as [shutdown](#socketshutdown-) of stream sockets.

### `terminal:setmode (mode)`

Sets the terminal mode,
according the following values of `mode`:

- `"normal"`: normal terminal mode
- `"raw"`: raw input mode (on Windows, [ENABLE_WINDOW_INPUT](https://docs.microsoft.com/en-us/windows/console/setconsolemode) is also enabled)
- `"binary"`: binary-safe I/O mode for IPC (Unix-only)

### `terminal:winsize ()`

Returns the width and height of the current terminal window size (in number of characters).

System Information
------------------

This section describes functions of `coutil.system` for obtaining information provided or maintained by the underlying operating system.

### `system.procinfo (which)`

Returns values corresponding to information about the current process and the system where it is running.
The following characters in string `which` define the values to be returned:

| Character | Value | Description |
| --------- | ----- | ----------- |
| `e` | string  <!-- uv_exepath --> | **executable** file path of the process (see note below). |
| `n` | string  <!-- uv_os_gethostname --> | **network** host name of the system. |
| `T` | string  <!-- uv_os_tmpdir --> | current **temporary** directory path. |
| `h` | string  <!-- name.machine --> | operating system **hardware** name. |
| `k` | string  <!-- name.sysname --> | operating system **kernel** name. |
| `v` | string  <!-- name.release --> | operating system _release_ **version** name. |
| `V` | string  <!-- name.version --> | operating system **version** _name_. |
| `$` | string  <!-- user.shell --> | current user **shell** path. |
| `H` | string  <!-- user.homedir --> | current user **home** directory path. |
| `U` | string  <!-- user.username --> | current **user** _name_. |
| `u` | integer <!-- user.uid --> | current **user** _identifier_ (uid). |
| `g` | integer <!-- user.gid --> | current user **group** identifier (gid). |
| `=` | integer <!-- usage.ru_isrss --> | integral unshared **stack** size of the process (bytes). |
| `d` | integer <!-- usage.ru_idrss --> | integral unshared **data** size of the process (bytes). |
| `m` | integer <!-- usage.ru_ixrss --> | _integral_ shared **memory** size of the process (bytes). |
| `R` | integer <!-- usage.ru_maxrss --> | _maximum_ **resident** set size of the process (bytes). |
| `<` | integer <!-- usage.ru_msgrcv --> | IPC messages **received** by the process. |
| `>` | integer <!-- usage.ru_msgsnd --> | IPC messages **sent** by the process. |
| `i` | integer <!-- usage.ru_inblock --> | block **input** operations of the process. |
| `o` | integer <!-- usage.ru_oublock --> | block **output** operations of the process. |
| `p` | integer <!-- usage.ru_minflt --> | **page** _reclaims_ (soft page faults) of the process. |
| `P` | integer <!-- usage.ru_majflt --> | **page** _faults_ (hard page faults) of the process. |
| `S` | integer <!-- usage.ru_nsignals --> | **signals** received by the process. |
| `w` | integer <!-- usage.ru_nswap --> | **swaps** of the process. |
| `x` | integer <!-- usage.ru_nvcsw --> | _voluntary_ **context** switches of the process. |
| `X` | integer <!-- usage.ru_nivcsw --> | _involuntary_ **context** switches of the process. |
| `c` | number  <!-- usage.ru_utime --> | user **CPU** time used of the process (seconds). |
| `s` | number  <!-- usage.ru_stime --> | **system** CPU time used of the process (seconds). |
| `1` | number  <!-- load[0] --> | system load of last **1 minute**. |
| `l` | number  <!-- load[1] --> | system **load** of last _5 minutes_. |
| `L` | number  <!-- load[2] --> | system **load** of last _15 minutes_. |
| `t` | number  <!-- uv_uptime --> | current system **uptime** (seconds). |
| `#` | integer <!-- uv_os_getpid --> | current process identifier (**pid**). |
| `^` | integer <!-- uv_os_getppid --> | **parent** process identifier (pid). |
| `b` | integer <!-- uv_get_total_memory --> | total amount of physical memory in the system (**bytes**). |
| `f` | integer <!-- uv_get_free_memory --> | amount of **free** memory available in the system (bytes). |
| `M` | integer <!-- uv_get_constrained_memory --> | _limit_ of **memory** available to the process (bytes). |
| `r` | integer <!-- uv_resident_set_memory --> | **resident** memory size of the process (bytes). |

This function never [fails](#failures).
It raises errors instead.

**Note**: option `"e"` may raise an error on Unix and AIX systems when used in the [standard standalone interpreter](http://www.lua.org/manual/5.4/manual.html#7) or any program that does not execute [`uv_setup_args`](http://docs.libuv.org/en/v1.x/misc.html#c.uv_setup_args) properly.

### `system.cpuinfo (which)`

Returns an [iterator](http://www.lua.org/manual/5.4/manual.html#3.3.5) that produces information about each individual CPU of the system in each iteration.
The _control variable_ is the index of the CPU.
The values of the other _loop variables_ are given by the following characters in string `which`:

| Character | Value | Description |
| --------- | ----- | ----------- |
| `m` | string | CPU **model** name. |
| `c` | integer | current CPU **clock** speed (MHz). |
| `u` | integer | time the CPU spent executing normal processes in **user** mode (milliseconds). |
| `n` | integer | time the CPU spent executing prioritized (**niced**) processes in user mode (milliseconds). |
| `s` | integer | time the CPU spent executing processes in **kernel** mode (milliseconds). |
| `d` | integer | time the CPU spent servicing **device** interrupts. (milliseconds). |
| `i` | integer | time the CPU was **idle** (milliseconds). |

This function never [fails](#failures).
It raises errors instead.

**Note**: the _state value_ returned is `which`,
and it only dictates how the values are returned in each call,
because the _iterator_ contains all the information that can be produced.
The initial value for the _control variable_ is `0`.
Also,
the _iterator_ is the _closing value_ used,
therefore it can be used in a [_to-be-closed_ variable](http://www.lua.org/manual/5.4/manual.html#3.3.8).
Finally,
the _iterator_ supports the length operator `#` to obtain the number of elements to be iterated.

### `system.netinfo (option, which)`

Returns values according to the value of `option`,
as described below:

- `"name"`: the name of the network interface corresponding to the interface index `which`.
- `"id"`: a string with the identifier suitable for use in an IPv6 scoped address corresponding to the interface index `which`.
- `"all"`: an [iterator](http://www.lua.org/manual/5.4/manual.html#3.3.5) that produces information about the addresses of the network interfaces of the system.
The _control variable_ is the index of the network interface address,
not the interface index.
The values of the other _loop variables_ are given by the following characters in string `which`:

| Character | Value | Description |
| --------- | ----- | ----------- |
| `n` | string | the name of the network interface. |
| `i` | boolean | `true` if the network interface address is internal, or `false` otherwise. |
| `d` | string | the domain of the network interface address (`"ipv4"` or `"ipv6"`). |
| `l` | integer | the length of the subnet mask of the network interface address. |
| `m` | string | the binary representation of the subnet mask of the network interface address. |
| `t` | string | the text representation of the network interface address. |
| `b` | string | the binary representation of the network interface address. |
| `T` | string | the text representation of the physical address (MAC) of the network interface. |
| `B` | string | the binary representation physical address (MAC) of the network interface. |

**Note**: this _iterator_ have the same characteristics of the one returned by [`system.cpuinfo`](#systemcpuinfo-which).

### `system.random (buffer [, i [, j [, mode]]])`

[Await function](#await-function) that awaits until it fills [memory](https://github.com/renatomaia/lua-memory) `buffer` with [cryptographically strong random bytes](https://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator),
from position `i` until `j`,
following the same sematics of these arguments in functions of [memory](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#lua-module).

If string `mode` is provided with character `~`,
it executes in [blocking mode](#blocking-mode).

In case of success,
this function returns `buffer`.

**Note**: this function may not complete when the system is [low on entropy](http://docs.libuv.org/en/v1.x/misc.html#c.uv_random).

Index
=====

<table><tr><td>
<a href='#channels'><code>coutil.channel</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#channelclose-ch'><code>channel.close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#channelcreate-name'><code>channel.create</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#channelgetname-ch'><code>channel.getname</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#channelgetnames-names'><code>channel.getnames</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#channelsync-ch-endpoint-'><code>channel.sync</code></a><br>
<a href='#state-coroutines'><code>coutil.coroutine</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#coroutineclose-co'><code>coroutine.close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#coroutineload-chunk--chunkname--mode'><code>coroutine.load</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#coroutineloadfile-filepath--mode'><code>coroutine.loadfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#coroutinestatus-co'><code>coroutine.status</code></a><br>
<a href='#events'><code>coutil.event</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventawait-e'><code>event.await</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventawaitall-e1-'><code>event.awaitall</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventawaitany-e1-'><code>event.awaitany</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventemitall-e-'><code>event.emitall</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventemitone-e-'><code>event.emitone</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#eventpending-e'><code>event.pending</code></a><br>
<a href='#mutex'><code>coutil.mutex</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#mutexislocked-e'><code>mutex.islocked</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#mutexlock-e'><code>mutex.lock</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#mutexownlock-e'><code>mutex.ownlock</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#mutexunlock-e'><code>mutex.unlock</code></a><br>
<a href='#promises'><code>coutil.promise</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#promiseawaitall-p-'><code>promise.awaitall</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#promiseawaitany-p-'><code>promise.awaitany</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#promisecreate-'><code>promise.create</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#promiseonlypending-p-'><code>promise.onlypending</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#promisepickready-p-'><code>promise.pickready</code></a><br>
<a href='#queued-events'><code>coutil.queued</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedawait-e'><code>queued.await</code></a><br>
</td><td>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedawaitall-e1-'><code>queued.awaitall</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedawaitany-e1-'><code>queued.awaitany</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedemitall-e-'><code>queued.emitall</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedemitone-e-'><code>queued.emitone</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedisqueued-e'><code>queued.isqueued</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#queuedpending-e'><code>queued.pending</code></a><br>
<a href='#coroutine-finalizers'><code>coutil.spawn</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#spawncatch-h-f-'><code>spawn.catch</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#spawntrap-h-f-'><code>spawn.trap</code></a><br>
<a href='#system-features'><code>coutil.system</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemaddress-type--data--port--mode'><code>system.address</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemawaitch-ch-endpoint-'><code>system.awaitch</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemawaitsig-signal'><code>system.awaitsig</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemcopyfile-path-destiny--mode'><code>system.copyfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemcpuinfo-which'><code>system.cpuinfo</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systememitsig-pid-signal'><code>system.emitsig</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemexecute-cmd-'><code>system.execute</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemfilebits'><code>system.filebits</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemfileinfo-path-mode'><code>system.fileinfo</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemfindaddr-name--service--mode'><code>system.findaddr</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesclose-'><code>addresses:close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesgetaddress-address'><code>addresses:getaddress</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesgetdomain-'><code>addresses:getdomain</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesgetsocktype-'><code>addresses:getsocktype</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesnext-'><code>addresses:next</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#addressesreset-'><code>addresses:reset</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemgetdir-'><code>system.getdir</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemgetenv-name'><code>system.getenv</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemgetpriority-pid'><code>system.getpriority</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemgrantfile-path-perm--mode'><code>system.grantfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemhalt-'><code>system.halt</code></a><br>
</td><td>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemisrunning-'><code>system.isrunning</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemlinkfile-path-destiny--mode'><code>system.linkfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemlistdir-path--mode'><code>system.listdir</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemmakedir-path-perm--mode'><code>system.makedir</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemmaketemp-prefix--mode'><code>system.maketemp</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemmovefile-path-destiny--mode'><code>system.movefile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemnameaddr-address--mode'><code>system.nameaddr</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemnanosecs-'><code>system.nanosecs</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemnetinfo-option-which'><code>system.netinfo</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemopenfile-path--mode--perm'><code>system.openfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileclose-mode'><code>file:close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileflush-mode'><code>file:flush</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#filegrant-perm--mode'><code>file:grant</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileinfo-mode'><code>file:info</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileown-uid-gid--mode'><code>file:own</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileread-buffer--i--j--offset--mode'><code>file:read</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#fileresize-size--mode'><code>file:resize</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#filetouch-mode-times'><code>file:touch</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#filewrite-data--i--j--offset--mode'><code>file:write</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemownfile-path-uid-gid--mode'><code>system.ownfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systempackenv-vars'><code>system.packenv</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemprocinfo-which'><code>system.procinfo</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemrandom-buffer--i--j--mode'><code>system.random</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemremovefile-path--mode'><code>system.removefile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemresume-co-'><code>system.resume</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemrun-mode'><code>system.run</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemsetdir-path'><code>system.setdir</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemsetenv-name-value'><code>system.setenv</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemsetpriority-pid-value'><code>system.setpriority</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemsocket-type-domain'><code>system.socket</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketaccept-'><code>socket:accept</code></a><br>
</td><td>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketbind-address'><code>socket:bind</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketclose-'><code>socket:close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketconnect-address'><code>socket:connect</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketgetaddress-site--address'><code>socket:getaddress</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketgetdomain-'><code>socket:getdomain</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketlisten-backlog'><code>socket:listen</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketread-buffer--i--j--address'><code>socket:read</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketsetoption-name-value-'><code>socket:setoption</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketshutdown-'><code>socket:shutdown</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#socketwrite-data--i--j--address'><code>socket:write</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemstdinstdoutstderr'><code>system.stdin|stdout|stderr</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalclose-'><code>terminal:close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalread-buffer--i--j'><code>terminal:read</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalsetmode-mode'><code>terminal:setmode</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalshutdown-'><code>terminal:shutdown</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalwinsize-'><code>terminal:winsize</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<a href='#terminalwrite-data--i--j'><code>terminal:write</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemsuspend-seconds--mode'><code>system.suspend</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemtime-mode'><code>system.time</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemtouchfile-path--mode-times'><code>system.touchfile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#systemunpackenv-env--tab'><code>system.unpackenv</code></a><br>
<a href='#thread-pools'><code>coutil.threads</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadsclose-pool'><code>threads.close</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadscount-pool-options'><code>threads.count</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadscreate-size'><code>threads.create</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadsdofile-pool-filepath--mode-'><code>threads.dofile</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadsdostring-pool-chunk--chunkname--mode-'><code>threads.dostring</code></a><br>
&nbsp;&nbsp;&nbsp;&nbsp;<a href='#threadsresize-pool-size--create'><code>threads.resize</code></a><br>
<br>
<br>
<br>
</td></tr></table>
