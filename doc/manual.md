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
- [`coutil.system`](#system)
	- [`system.run`](#systemrun-mode)
	- [`system.pause`](#systempause-delay)
	- [`system.awaitsig`](#systemawaitsig-signal)
	- [`system.address`](#systemaddress-type--data--port--mode)
	- [`system.tcp`](#systemtcp-type--domain)
	- [`tcp:close`](#tcpclose-)
	- [`tcp:getdomain`](#tcpgetdomain-)
	- [`tcp:bind`](#tcpbind-address)
	- [`tcp:getaddress`](#tcpgetaddress-site--address)
	- [`stream:setoption`](#streamsetoption-name-value)
	- [`stream:getoption`](#streamgetoption-name)
	- [`stream:connect`](#streamconnect-address)
	- [`stream:send`](#streamsend-data--i--j)
	- [`stream:receive`](#streamreceive-buffer--i--j)
	- [`stream:shutdown`](#streamshutdown-)
	- [`listen:listen`](#listenlisten-backlog)
	- [`listen:accept`](#listenaccept-)

Contents
========

Events
------

Module `coutil.event` provides functions for synchronization of coroutines using events.
Events can be emitted on any value that can be stored in a table as a key (all values except `nil`).
Coroutines might suspend awaiting for events on values, so they are resumed when events are emitted on these values.

### `event.await (e)`

Suspends the execution of the calling coroutine awaiting an event on value `e`.

If it is resumed by [`emitone`](#eventemitone-e-) or [`emitall`](#eventemitall-e-), it returns `e` followed by all the additional arguments passed to these functions.
Otherwise it returns the values provided to the resume (_e.g._ [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume)).

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

It returns like [`event.await`](#eventawait-e).

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
The resumed coroutine shall later emit an event on `e` to resume any remaning threads waiting for the onwership.

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

If there are no fulfilled promises in `p, ...`, it suspends the calling coroutine awaiting the fulfillment of any of the promises in `p, ...`.
If the calling coroutine is resumed by the fullfillment of a promise in `p, ...`, it returns the fulfilled promise.
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

System
------

Module `coutil.system` provides functions to suspend coroutines and schedule them to be resumed when they are ready according to a system condition.

### `system.run ([mode])`

Resumes scheduled coroutines that becomes ready according to its corresponding system condition.

`mode` is a string that defines how `run` executes, as described below:

- `"loop"` (default): it executes continously resuming every coroutine that becomes ready until there are no more scheduled coroutines.
- `"step"`: it resumes every ready coroutine once, or waits to resume at least one coroutine that becomes ready.
- `"ready"`: it resumes only coroutines that are currently ready.

`run` returns `true` if there are scheduled coroutines, or `false` otherwise.

### `system.pause ([delay])`

Suspends the execution of the calling coroutine, and schedules it to be resumed after `delay` seconds have passed since the coroutine was last resumed.

If `delay` is not provided or is `nil`, the coroutine is scheduled as ready, so it will be resumed as soon as possible.
The same is applies when `delay` is zero or negative.

`pause` returns `true` if the calling coroutine is resumed as scheduled.
Otherwise it returns like [`event.await`](#eventawait-e).
In any case, the coroutine is not scheduled to be resumed anymore after it returns.

### `system.awaitsig (signal)`

Suspends the execution of the calling coroutine (like [`coroutine.yield`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield)) but also schedules it to be resumed when the process receives signal indicated by string `signal`, as listed below:

- Process Commands

| `signal`      | UNIX Name | Action    | Indication |
| ------------- | --------- | --------- | ---------- |
| `"abort"`     | SIGABRT   | core dump | Process shall **abort**. |
| `"continue"`  | SIGCONT   | continue  | Process shall **continue**, if stopped. |
| `"terminate"` | SIGTERM   | terminate | Process shall **terminate**. |

- Terminal Interaction

| `signal`      | UNIX Name | Action    | Indication |
| ------------- | --------- | --------- | ---------- |
| `"bgread"`    | SIGTTIN   | stop      | **Read** from terminal while in **background**. |
| `"bgwrite"`   | SIGTTOU   | stop      | **Write** to terminal while in **backgroun**d. |
| `"hangup"`    | SIGHUP    | terminate | Terminal was closed. |
| `"interrupt"` | SIGINT    | terminate | Terminal requests the process to terminate. (_e.g._ `Ctrl+C`) |
| `"quit"`      | SIGQUIT   | core dump | Terminal requests the process to **quit** with a [core dump](https://en.wikipedia.org/wiki/Core_dump). |
| `"stop"`      | SIGTSTP   | stop      | Terminal requests the process to **stop**. (_e.g._ `Ctrl+Z`) |
| `"winresize"` | SIGWINCH  | ignore    | Terminal window size has changed. |

- Standard Notifications

| `signal`      | UNIX Name | Action    | Indication |
| ------------- | --------- | --------- | ---------- |
| `"child"`     | SIGCHLD   | ignore    | **Child** process terminated, stopped, or continued. |
| `"clocktime"` | SIGALRM   | terminate | Real or **clock time** elapsed. |
| `"cpulimit"`  | SIGXCPU   | core dump | Defined **CPU** time **limit** exceeded. |
| `"cputimall"` | SIGPROF   | terminate | **CPU time** used by the **process** and by the **system on behalf of the process** elapses. |
| `"cputimprc"` | SIGVTALRM | terminate | **CPU time** used by the **process** elapsed.  |
| `"debug"`     | SIGTRAP   | core dump | Exception or **debug** trap occurs. |
| `"filelimit"` | SIGXFSZ   | core dump | Allowed **file** size **limit** exceeded. |
| `"loosepipe"` | SIGPIPE   | terminate | Write on a **pipe** with no one to read it. |
| `"polling"`   | SIGPOLL   | terminate | Event occurred on [watched file descriptor](https://pubs.opengroup.org/onlinepubs/9699919799/functions/ioctl.html). |
| `"sysargerr"` | SIGSYS    | core dump | System call with a bad argument. |
| `"urgsock"`   | SIGURG    | core dump | High-bandwidth data is available at a socket. |
| `"user1"`     | SIGUSR1   | terminate | User-defined conditions. |
| `"user2"`     | SIGUSR2   | terminate | User-defined conditions. |

`awaitsig` returns like [`pause`](#systempause-).

### `system.address (type [, data [, port [, mode]]])`

Returns a new IP address structure.
`type` is either the string `"ipv4"` or `"ipv6"`,
to indicate the address to be created shall be a IPv4 or IPv6 address, respectively.

If `data` is not provided the structure created is initilized with null data:
`0.0.0.0:0` for `"ipv4"`, or `[::]:0` for `"ipv6"`.
Otherwise, `data` is a string with the information to be stored in the structure created.

If only `data` is provided, it must be a literal address as formatted inside a URI,
like `"192.0.2.128:80"` (IPv4), or `"[::ffff:c000:0280]:80"` (IPv6).
Moreover, if `port` is provided, `data` is a host address and `port` is a port number to be used to initialize the address structure.
The string `mode` controls whether `data` is text (literal) or binary.
It may be the string `"b"` (binary data), or `"t"` (text data).
The default is `"t"`.

The returned object provides the following fields:

- `type`: is either the string `"ipv4"` or `"ipv6"`,
to indicate the address is a IPv4 or IPv6 address, respectively.
- `literal`: is the text (literal) representation of the address,
like `"192.0.2.128"` (IPv4) or `"::ffff:c000:0280"` (IPv6).
- `binary`: is the binary representation of the address,
like `"\192\0\2\128"` (IPv4) or `"\0\0\0\0\0\0\0\0\0\0\xff\xff\xc0\x00\x02\x80"` (IPv6).
- `port`: is the port number of the IPv4 and IPv6 address.

Moreover, you can pass the object to the standard function `tostring` to obtain the address as a string inside a URI,
like `"192.0.2.128:80"` (IPv4) or `[::ffff:c000:0280]:80` (IPv6).

### `system.tcp ([type [, domain]])`

Creates a TCP socket, of the type specified by `type`,
which is either:

- `"stream"` creates stream socket for data transfers.
- `"listen"` creates socket to accept stream socket connections.

The `domain` string defines the socket's address domain (or family),
and can be  either `"ipv4"` or `"ipv6"`,
to create socket on the IPv4 or IPv6 address domain.

On success it returns a new socket,
or `nil` plus an error message otherwise.

### `tcp:close ()`

Closes socket `tcp`.
Note that sockets are automatically closed when their handles are garbage collected,
but that takes an unpredictable amount of time to happen. 

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `tcp:getdomain ()`

Returns the address domain of `tcp`, which can be either `"ipv4"` `"ipv6"`.

### `tcp:bind (address)`

Binds socket `tcp` to the local address provided as `address`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `tcp:getaddress ([site [, address]])`

Returns the address associated with socket `tcp`, as indicated by `site`, which can be:

- `"this"`: The socket's address (the default).
- `"peer"`: The socket's peer address.

If `address` is provided, it is the address structure used to store the result,
otherwise a new `address` object is returned with the result data.

In case of errors, it returns `nil` plus an error message.

### `stream:setoption (name, value)`

Sets the option `name` for socket `tcp`.
The available options are the same as defined in operation [`socket:getoption`].

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `stream:getoption (name)`

Returns the value of option `name` of `socket`.
There available options are:

- `"keepalive"`: is a number of seconds of the initial delay of the periodic transmission of messages when the TCP keep-alive option is enabled for socket `tcp`,
or `nil` otherwise.
- `"nodelay"`: is `true` when coalescing of small segments shall be avoided in `socket`,
or `false` otherwise.

### `stream:connect (address)`

Binds socket `stream` to the peer address provided as `address`.

This operation is not available for sockets of type `listen`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `stream:send (data [, i [, j]])`

Sends the substring of `data` that starts at `i` and continues until `j`;
`i` and `j` can be negative.
If `j` is absent,
it is assumed to be equal to -1 (which is the same as the string length).

This operation is not available for sockets of type `listen`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

__Note__: if `data` is a [memory](https://github.com/renatomaia/lua-memory), it is not converted to a Lua string prior to have its specified contents transfered.

### `stream:receive (buffer [, i [, j]])`

Receives from socket `stream` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`;
`i` and `j` can be negative.
If `j` is absent, it is assumed to be equal to -1
(which is the same as the buffer size).

This operation is not available for sockets of type `listen`.

Returns the number of bytes actually received from `socket` in case of success.
Otherwise it returns `nil` plus an error message.

### `stream:shutdown ()`

Shuts down the write side of socket `stream`.

This operation is only available for `stream` sockets.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `listen:listen (backlog)`

Starts listening for new connections on socket `listen`.
`backlog` is a hint for the underlying system about the suggested number of outstanding connections that shall be kept in the socket's listen queue.

This operation is only available for sockets of type `listen`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `listen:accept ()`

Accepts a new pending connection on socket `listen`.

This operation is only available for sockets of type `listen`.

Returns a new `stream` TCP socket for the accepted connection in case of success.
Otherwise it returns `nil` plus an error message.
