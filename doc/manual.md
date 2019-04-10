Index
=====

- [`coutil.event`](#events)
	- [`event.await`](#eventawait-e)
	- [`event.awaitall`](#eventawaitall-e1-)
	- [`event.awaitany`](#eventawaitany-e1-)
	- [`event.emitall`](#eventemitall-e-)
	- [`event.emitone`](#eventemitone-e-)
	- [`event.pending`](#eventpending-e)
- [`coutil.queued`](#queued-events)
	- [`queued.await`](#queuedawait-e)
	- [`queued.awaitall`](#queuedawaitall-e1-)
	- [`queued.awaitany`](#queuedawaitany-e1-)
	- [`queued.emitall`](#queuedemitall-e-)
	- [`queued.emitone`](#queuedemitone-e-)
	- [`queued.pending`](#queuedpending-e)
	- [`queued.isqueued`](#queuedisqueued-e)
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
- [`coutil.spawn`](#spawn)
	- [`spawn.catch`](#spawncatch-h-f-)
	- [`spawn.trap`](#spawntrap-h-f-)
- [`coutil.scheduler`](#scheduler)
	- [`scheduler.run`](#schedulerrun-mode)
	- [`scheduler.pause`](#schedulerpause-delay-)
	- [`scheduler.awaitsig`](#schedulerawaitsig-signal-)

Contents
========

Events
------

Module `coutil.event` provides functions for synchronization of coroutines using events.
Events can be emitted on any value that can be stored in a table as a key (all values except `nil`).
Coroutines might suspend awaiting for events on values, so they are resumed when events are emitted on these values.

### `event.await (e)`

Suspends the execution of the calling coroutine awaiting an event on value `e`.

If it is resumed by [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-), it returns `true` followed by all the additional arguments passed to these functions.
Otherwise it returns `false` followed by the values provided to the resume (_e.g._ [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume)).

### `event.awaitall ([e1, ...])`

Suspends the execution of the calling coroutine awaiting an event on every one of values `e1, ...`.
Any `nil` in `e1, ...` is ignored.
Any repeated values in `e1, ...` are treated as a single one.
If `e1, ...` are not provided or are all `nil`, this function has no effect.

It returns `true` if the calling coroutine is resumed due to events emitted on all values `e1, ...` or if `e1, ...` are not provided or are all `nil`.
Otherwise it returns like [`event.await`](#eventawait-e).

### `event.awaitany (e1, ...)`

Suspends the execution of the calling coroutine awaiting an event on any of the values `e1, ...`.
Any `nil` in `e1, ...` is ignored.
At least one value in `e1, ...` must not be `nil`.

If the calling coroutine is resumed due to an event emitted on any of values `e1, ...`, `awaitany` returns `true` followed by any additional values passed to [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-).
Otherwise it returns like [`event.await`](#eventawait-e).

### `event.emitall (e, ...)`

Resumes all coroutines waiting for event `e` in the same order they were suspended.
The additional arguments `...` are passed to every resumed coroutine.
This function returns after resuming all coroutines awaiting event `e` at the moment its call is initiated.
It returns `true` if there was some coroutine awaiting the event, or `false` otherwise.

### `event.emitone (e, ...)`

Resumes one single coroutine waiting for event `e`, if there is any.
The additional arguments `...` are passed to the resumed coroutine.
This function returns after resuming the coroutine awaiting event `e`.
It returns `true` if there was some coroutine awaiting the event, or `false` otherwise.

### `event.pending (e)`

Returns `true` if there is some coroutine suspended awaiting for event `e`, or `false` otherwise.

Queued Events
-------------

Module `coutil.queued` provides functions similar to module `coutil.event`, however the function in this module store events emitted in a queue to be consumed later as if they were emitted immediately after a coroutine awaits for them.

### `queued.await (e)`

Same as [`event.await`](#eventawait-e), but it consumes a stored event emitted on value `e`, if there is any.

### `queued.awaitall ([e1, ...])`

Same as [`event.awaitall`](#eventawaitall-e1-), but it first attempts to consume one stored event on each value `e1, ...`, and only await on the values `e1, ...` that do not have a stored event.

### `queued.awaitany (e1, ...)`

Same as [`event.awaitany`](#eventawaitany-e1-), but if there is a stored event on any of values `e1, ...`, the stored event on the leftmost value `e1, ...` is consumed instead of awaiting for events.

### `queued.emitall (e, ...)`

Same as [`event.emitall`](#eventemitall-e-), but if there is no coroutine awaiting on `e`, it stores the event to be consumed later.

### `queued.emitone (e, ...)`

Same as [`event.emitone`](#eventemitone-e-), but if there is no coroutine awaiting on `e`, it stores the event to be consumed later.

### `queued.pending (e)`

Alias for [`event.pending`](#eventpending-e).

### `queued.isqueued (e)`

Returns `true` if there is some stored event on `e`, or `false` otherwise.

Mutex
-----

Module `coutil.mutex` provides functions for mutual exclusion of coroutines when using a resource.

### `mutex.islocked (e)`

Returns `true` if the exclusive ownership identified by value `e` is taken, and `false` otherwise.

### `mutex.lock (e)`

Acquires to the current coroutine the exclusive ownership identified by value `e`.
If the ownership is not taken then the function acquires the ownership and returns immediately.
Otherwise it awaits an event on value `e` (see [`coutil.event`](#events)) until the ownership is released so it can be acquired to the coroutine.

### `mutex.ownlock (e)`

Returns `true` if the exclusive ownership identified by value `e` belongs to the calling coroutine, and `false` otherwise.

### `mutex.unlock (e)`

Releases from the current coroutine the exclusive ownership identified by value `e`.
It also emits an event on `e` (see [`coutil.event`](#events)) to resume one of the coroutines awaiting to acquire this ownership.
The resumed coroutine shall later emit an event on `e` to resume any remaning threads waiting for the onwership, which is automatically done by calling [`unlock`](#mutexunlock-e) after the call to [`lock`](#mutexlock-e) returns.

Promises
--------

Module `coutil.promise` provides functions for synchronization of coroutines using promises.
Promises are used to obtain results that will only be available at a later moment, when the promise is fulfilled.
Coroutines that claim an unfulfilled promise suspend awaiting its fulfillment in order to receive the results.
But once a promise is fulfilled its results become readily available for those that claims them.

### `promise.awaitall ([p, ...])`

Suspends the calling coroutine awaiting the fulfillment of all promises `p, ...`.
Returns `true` if all promises `p, ...` are fulfilled.
Otherwise it returns like [`event.await`](#eventawait-e).

### `promise.awaitany (p, ...)`

Returns a fulfilled promise from promises `p, ...`.
If there are no fulfilled promises in `p, ...`, it suspends the calling coroutine awaiting the fulfillment of any of the promises in `p, ...`.
Returns `true` followed by the fulfilled promise in `p, ...`.
Otherwise it returns like [`event.await`](#eventawait-e).

### `promise.create ()`

Returns a new promise, followed by a fulfillment function.

The promise is a function that returns the promise's results.
If the promise is unfulfilled, it suspends the calling coroutine awaiting its fulfillment.
If the promise is called with an argument that evaluates to `true`, it never suspends the calling coroutine, and just returns `true` if the promise is fulfiled, or `false` otherwise.

The fulfillment function shall be called in order to fulfill the promise.
The arguments passed to the fulfillment function become the promise's results.
It returns `true` the first time it is called, or `false` otherwise.

Coroutines suspeded awaiting a promise's fulfillment are actually suspended awaiting an event on the promise (see [`await`](#eventawait-e)).
Such coroutines can be resumed by emitting an event on the promise.
In such case, the additional values to [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-) will be returned by the promise.
But the promise will remain unfulfilled.
Therefore, when the promise is called again, it will suspend the calling coroutine awaiting an event on the promise (the promise fulfillment).

The first time the fulfillment function is called, an event is emitted on the promise with the promise's results as additional values.
Therefore, to suspend awaiting the fulfillment of a promise a coroutine may await an event on the promise.
However, once a promise is fulfilled, any coroutine that suspends awaiting events on the promise will remain suspended until an event is on the promise is emitted with function [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-).

### `promise.onlypending ([p, ...])`

Returns all promises `p, ...` that are unfulfilled.

### `promise.pickready ([p, ...])`

Returns the first promise `p, ...` that is fulfilled, or no value if none of promises `p, ...` is fulfilled.

Spawn
-----

Module `coutil.spawn` provides functions to execute functions in new coroutines with associated handler functions to deal with the results.

### `spawn.catch (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If any error is raised inside `f`, the coroutine executes the error message handler function `h` with the error message as argument.
`h` is executed in the calling context of the raised error, just like an error message handler in `xpcall`.

### `spawn.trap (h, f, ...)`

Calls function `f` with the given arguments in a new coroutine.
If `f` executes without any error, the coroutine executes function `h` passing as arguments the `true` followed by all the results from `f`.
In case of any error, `h` is executed with arguments `false` and the error message.
In the latter case, `h` is executed in the calling context of the raised error, just like a error message handler in `xpcall`.

Scheduler
---------

Module `coutil.scheduler` provides functions to suspend coroutines and schedule them to be resumed when they are ready according to a system condition.

### `scheduler.run ([mode])`

Resumes scheduled coroutines that becomes ready according to its corresponding system condition.

`mode` is a string that defines how `run` executes, as described below:

- `"loop"` (default): it executes continously resuming every coroutine that becomes ready until there are no more scheduled coroutines.
- `"step"`: it resumes every ready coroutine once, or waits to resume at least one coroutine that becomes ready.
- `"ready"`: it resumes only coroutines that are currently ready.

`run` returns `true` if there are scheduled coroutines, or `false` otherwise.

### `scheduler.pause ([delay, ...])`

Suspends the execution of the calling coroutine (like [`coroutine.yield`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield)) but also schedules it to be resumed after `delay` seconds have passed since the coroutine was last resumed.
Any arguments to `pause` are passed as extra results to [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume).

If `delay` is not provided or is `nil`, the coroutine is scheduled as ready, so it will be resumed as soon as possible.
The same is applies when `delay` is zero or negative.

`pause` returns `true` if the calling coroutine is effectively resumed by [`run`](#schedulerrun-mode).
Otherwise it returns like [`event.await`](#eventawait-e).
In any case, the coroutine is not scheduled to be resumed anymore after `pause` returns.

### `scheduler.awaitsig (signal, ...)`

Suspends the execution of the calling coroutine (like [`coroutine.yield`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield)) but also schedules it to be resumed when the process receives signal indicated by string `signal`, as listed below:

| Command       | UNIX Name | Description |
| ------------- | --------- | ----------- |
| `"abort"`     | SIGABRT   | Process abort signal |
| `"continue"`  | SIGCONT   | Continue executing, if stopped |
| `"hangup"`    | SIGHUP    | Hangup |
| `"interrupt"` | SIGINT    | Terminal interrupt signal |
| `"quit"`      | SIGQUIT   | Terminal quit signal |
| `"stop"`      | SIGTSTP   | Terminal stop signal |
| `"terminate"` | SIGTERM   | Termination signal |

| Condition     | UNIX Name | Description |
| ------------- | --------- | ----------- |
| `"badpipe"`   | SIGPIPE   | Write on a pipe with no one to read it |
| `"bgread"`    | SIGTTIN   | Attempt to read while in background |
| `"bgwrite"`   | SIGTTOU   | Attempt to write while in background |
| `"cpulimit"`  | SIGXCPU   | CPU time limit exceeded |
| `"filelimit"` | SIGXFSZ   | File size limit exceeded |
| `"syscall"`   | SIGSYS    | Bad system call |

| Event         | UNIX Name | Description |
| ------------- | --------- | ----------- |
| `"child"`     | SIGCHLD   | Child process terminated, stopped, or continued |
| `"clocktime"` | SIGALRM   | Alarm clock |
| `"cputimprc"` | SIGVTALRM | Virtual timer expired  |
| `"cputimall"` | SIGPROF   | Profiling timer expired  |
| `"debug"`     | SIGTRAP   | Trace/breakpoint trap |
| `"polling"`   | SIGPOLL   | Pollable event |
| `"urgsock"`   | SIGURG    | High bandwidth data is available at a socket |
| `"user1"`     | SIGUSR1   | User-defined signal 1 |
| `"user2"`     | SIGUSR2   | User-defined signal 2 |
| `"winresize"` | SIGWINCH  | Terminal window size changed |

Any additional arguments to `awaitsig` are passed as extra results to [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume).

`awaitsig` returns like [`pause`](#schedulerpause-).
