Summary
=======

- [Await Functions](#await)
- [Events](#events)
- [Queued Events](#queued-events)
- [Mutexes](#mutex)
- [Promises](#promises)
- [Coroutine Finalizers](#spawn)
- [System Features](#system)

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
	- [`system.isrunning`](#systemisrunning-)
	- [`system.halt`](#systemhalt-)
	- [`system.time`](#systemtime-update)
	- [`system.nanosecs`](#systemnanosecs-)
	- [`system.suspend`](#systemsuspend-delay)
	- [`system.emitsig`](#systememitsig-pid-signal)
	- [`system.awaitsig`](#systemawaitsig-signal)
	- [`system.execute`](#systemexecute-cmd-)
	- [`system.address`](#systemaddress-type--data--port--mode)
	- [`system.findaddr`](#systemfindaddr-name--service--mode)
	- [`system.nameaddr`](#systemnameaddr-address--mode)
	- [`system.socket`](#systemsocket-type-domain)
		- [`socket:close`](#socketclose-)
		- [`socket:getdomain`](#socketgetdomain-)
		- [`socket:setoption`](#socketsetoption-name-value)
		- [`socket:getoption`](#socketgetoption-name)
		- [`socket:bind`](#socketbind-address)
		- [`socket:connect`](#socketconnect-address)
		- [`socket:getaddress`](#socketgetaddress-site--address)
		- [`socket:send`](#socketsend-data--i--j--address)
		- [`socket:receive`](#socketreceive-buffer--i--j--address)
		- [`socket:joingroup`](#socketjoingroup-multicast--interface)
		- [`socket:leavegroup`](#socketleavegroup-multicast--interface)
		- [`socket:shutdown`](#socketshutdown-)
		- [`socket:listen`](#socketlisten-backlog)
		- [`socket:accept`](#socketaccept-)
	- [`system.load`](#systemload-chunk--chunkname--mode)
	- [`system.loadfile`](#systemloadfile-filepath--mode)
		- [`syscoro:resume`](#syscororesume-)
		- [`syscoro:status`](#syscorostatus-)
		- [`syscoro:close`](#syscoroclose-)
	- [`system.threads`](#systemthreads-size)
		- [`threads:resize`](#threadsresize-size--create)
		- [`threads:count`](#threadsgetcount-option)
		- [`threads:dostring`](#threadsdostring-chunk--chunkname--mode-)
		- [`threads:dofile`](#threadsdofile-filepath--mode-)
		- [`threads:close`](#threadsclose-)
	- [`system.channelnames`](#systemchannelnames-action-)
	- [`system.channel`](#systemchannel-name)
		- [`channel:sync`](#channelsync-endpoint-)
		- [`channel:trysync`](#channeltrysync-endpoint-)
		- [`channel:probe`](#channelprobe-endpoint)
		- [`channel:close`](#channelclose-)

Contents
========

Await
-----

An _await function_ [suspends](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield) the execution of the calling coroutine,
but also implies that the coroutine will be resumed on some specitic condition.

Coroutines executing an _await function_ can be resumed explicitly by [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume).
In such case,
the _await function_ returns the values provided to the resume.
Otherwise,
the _await function_ returns as described in the following sections.
In any case,
the coroutine will not be implicitly resumed after the _await function_ returns.

Events
------

Module `coutil.event` provides functions for synchronization of coroutines using events.
Coroutines might suspend awaiting for events on any value (except `nil`) in order to be resumed when events are emitted on these values.

A coroutine awaiting an event on a value does not prevent the value nor the coroutine to be collected,
but the coroutine will not be collected as long as the value does not become garbage.

### `event.await (e)`

Equivalent to [`event.awaitany`](#eventawait-e)`(e)`.

### `event.awaitall ([e1, ...])`

[Await function](#await) that awaits an event on every one of values `e1, ...`.
Any `nil` in `e1, ...` is ignored.
Any repeated values in `e1, ...` are treated as a single one.
If `e1, ...` are not provided or are all `nil`, this function has no effect.

Returns `true` after events are emitted on all values `e1, ...`,
or if `e1, ...` are not provided or are all `nil`.

### `event.awaitany (e1, ...)`

[Await function](#await) that awaits an event on any of the values `e1, ...`.
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

Storing events on a value does not prevent the value nor the event arguments to be collected.

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

### `queued.pending (e)`

Alias for [`event.pending`](#eventpending-e).

### `queued.isqueued (e)`

Returns `true` if there is some stored event on `e`,
or `false` otherwise.

Mutex
-----

Module `coutil.mutex` provides functions for [mutual exclusion](https://en.wikipedia.org/wiki/Mutex) of coroutines when using a resource.

### `mutex.islocked (e)`

Returns `true` if the exclusive ownership identified by value `e` is taken,
and `false` otherwise.

### `mutex.lock (e)`

Acquires to the current coroutine the exclusive ownership identified by value `e`.
If the ownership is not taken then the function acquires the ownership and returns immediately.
Otherwise,
it awaits an [event](#events) on value `e` until the ownership is released
(see [`mutex.unlock`](#mutexunlock-e)),
so it can be acquired to the coroutine.

### `mutex.ownlock (e)`

Returns `true` if the exclusive ownership identified by value `e` belongs to the calling coroutine,
and `false` otherwise.

### `mutex.unlock (e)`

Releases from the current coroutine the exclusive ownership identified by value `e`.
It also emits an [event](#events) on `e` to resume one of the coroutines awaiting to acquire this ownership
(see [`mutex.lock`](#mutexlock-e)).

Promises
--------

Module `coutil.promise` provides functions for synchronization of coroutines using [promises](https://en.wikipedia.org/wiki/Futures_and_promises).
Promises are used to obtain results that will only be available at a later moment, when the promise is fulfilled.
Coroutines that claim an unfulfilled promise suspend awaiting its fulfillment in order to receive the results.
But once a promise is fulfilled its results become readily available for those that claims them.

### `promise.awaitall ([p, ...])`

[Await function](#await) that awaits the fulfillment of all promises `p, ...`.
Returns `true` if all promises `p, ...` are fulfilled.
Otherwise it returns like [`event.await`](#eventawait-e).

### `promise.awaitany (p, ...)`

[Await function](#await) that awaits the fulfillment of any of the promises in `p, ...`,
if there are no fulfilled promises in `p, ...`.
Otherwise it returns immediately.
In any case, it returns a fulfilled promise.

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
the coroutines can be resumed prematurely by [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume).
In such case,
the promisse returns the values provided to the resume
(like [`coroutine.yield`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield)).
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

### `promise.onlypending ([p, ...])`

Returns all promises `p, ...` that are unfulfilled.

### `promise.pickready ([p, ...])`

Returns the first promise `p, ...` that is fulfilled,
or no value if none of promises `p, ...` is fulfilled.

Spawn
-----

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
If `f` executes without any error, the coroutine executes function `h` passing as arguments the `true` followed by all the results from `f`.
In case of any error,
`h` is executed with arguments `false` and the error message.
In the latter case,
`h` is executed in the calling context of the raised error,
just like a error message handler in `xpcall`.
Returns the new coroutine.

System
------

Module `coutil.system` provides functions that expose system functionalities,
including [await functions]("#await") to await on system conditions.

Unless otherwise stated,
all there functions return `nil` plus an error message on failure,
and some value different from `nil` on success.

### `system.run ([mode])`

Resumes coroutines awaiting to system condition.

`mode` is a string that defines how `run` executes, as described below:

- `"loop"` (default): it executes continously resuming every awaiting coroutine that becomes ready to be resumed until there are no more awaiting coroutines.
- `"step"`: it resumes every ready coroutine once,
or waits to resume at least one coroutine that becomes ready.
- `"ready"`: it resumes only coroutines that are currently ready.

Returns `true` if there are remaining awaiting coroutines,
or `false` otherwise.

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

[Await function](#await) that awaits `delay` seconds since timestamp provided by [`system.time`](#systemtime-update).

If `delay` is not provided,
is `nil`,
or negative,
it is assumed as zero,
so the calling coroutine will be resumed as soon as possible.

Returns `true` in case of success.

### `system.emitsig (pid, signal)`

Emits signal indicated by string `signal` to process with id `pid`.
The strings that represent signals are described in [`system.awaitsig`](#systemawaitsig-signal).

### `system.awaitsig (signal)`

[Await function](#await) that awaits for the process to receive the signal indicated by string `signal`,
as listed below:

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
| `"interrupt"` | SIGINT    | terminate | Terminal requests the process to terminate. (_e.g._ Ctrl+`C`) |
| `"quit"`      | SIGQUIT   | core dump | Terminal requests the process to **quit** with a [core dump](https://en.wikipedia.org/wiki/Core_dump). (_e.g._ Ctrl+`\`) |
| `"stop"`      | SIGTSTP   | stop      | Terminal requests the process to **stop**. (_e.g._ Ctrl+`Z`) |
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

Returns string `signal`.

### `system.execute (cmd, ...)`

[Await function](#await) that executes a new process,
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

- `stdin`:
file or stream socket to be used as the standard input of the new process.

- `stdout`:
file or stream socket to be used as the standard output of the new process.

- `stderr`:
file or stream socket to be used as the standard error output of the new process.

- `arguments`:
table with the sequence of command-line arguments for the executable image of the new process.
When this field is not provided,
the new process's executable image receives no arguments.

- `environment`:
table mapping environment variable names to the values they must assume for the new process.
If this field is provided,
only the variables defined will be available for the new process.

If `cmd` is a table,
the field `pid` is set with a number that identifies the new process
(_e.g._ UNIX process identifier)
before the calling coroutine is suspended.

Returns the string `"exit"`,
followed by a number of the exit status
when the process terminates normally,
or the [name of the signal](#systemawaitsig-signal) that terminated the program,
followed by the platform-dependent number of the signal.

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

- `type`: is either the string `"ipv4"` or `"ipv6"`,
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

### `system.findaddr (name [, service [, mode]])`

[Await function](#await) that searches for the addresses of network name `name`,
and awaits for the addresses found.
If `name` is `nil`,
the loopback address is searched.
If `name` is `"*"`,
the wildcard address is searched.

`service` indicates the port number or service name to be used to resolve the port number of the resulting addresses.
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

Returns an iterator that have the following usage pattern,
followed by the type of the first address found,
or `nil` if no address is found.

```
	[address, socktype, nextdomain =] getnext ([address])
```

Each time the iterator is called,
returns one address found for node with `name`,
followed by the type of the socket to be used to connect to the address,
and the type of the next address.
If an address structure is provided as `address`,
it is used to store the result;
otherwise a new address structure is created.
Therefore,
`nextdomain` is `"ipv4"` if the next address is a IPv4 address,
or `"ipv6"` if the next address is a IPv6 address,
or `nil` if the next call will return no address.

As an example,
the following loop will iterate over all the addresses found for service named 'ssh' on node named `www.lua.org`,
using the same IPv4 address object:

```lua
getnext = assert(system.findaddr("www.lua.org", "ssh", "4"))
for addr, scktype in getnext, address.create("ipv4") do
	print(addr, scktype)
end
```

The next example gets only the first address found.

```lua
addr, scktype = system.findaddr("www.lua.org", "http", "s")()
```

Yet another example that collects all address found in new address objects.

```lua
list = {}
for addr, scktype in system.findaddr("www.lua.org", "http", "s") do
	list[#list+1] = addr
end
```

Finally,
an example that fills existing addreses objects with the results

```lua
address = { ipv4 = system.address("ipv4"), ipv6 = system.address("ipv6") }
getnext, nextdomain = assert(system.findaddr("www.lua.org", "http", "s"))
repeat
	addr, scktype, nextdomain = getnext(address[nextdomain])
until nextdomain == nil
```

### `system.nameaddr (address [, mode])`

[Await function](#await) that searches for a network name for `address`,
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
- `i`: for names the _Internationalized Domain Name_ (IDN) format.
- `u`: allows unassigned Unicode code points
(implies `i`).
- `a`: checks host name is conforming to STD3
(implies `i`).

By default,
`mode` is the empty string.

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
- `"ipc"` same as `"local"`,
but allows for transmission of stream sockets.

In case of success,
it returns the new socket,
Otherwise it returns `nil` plus an error message.

### `socket:close ()`

Closes socket `socket`.
Note that sockets are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:getdomain ()`

Returns the address domain of `socket`,
which can be either `"ipv4"`,
`"ipv6"`,
or `"local"`.

### `socket:setoption (name, value)`

Sets `value` as the value of option `name` for socket `socket`.
This operation is not available for passive TCP sockets.
The available options are the same as defined in operation [`socket:getoption`](#socketgetoption-name).

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:getoption (name)`

Returns the value of option `name` of socket `socket`.
This operation is not available for passive TCP sockets.
There available options are:

#### UDP Socket

- `"broadcast"`: is `true` when broadcast is enabled in `socket`,
or `false` otherwise.
- `"mcastloop"`: is `true` when loopback of outgoing multicast datagrams is enabled,
or `false` otherwise.
(default is `true`).
- `"mcastttl"`: is a number from 1 to 255 of multicast time to live.
(default is 1).
- `"mcastiface"`: is the address of the interface for multicast,
or `nil` if none is defined.

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

### `socket:bind (address)`

Binds socket `socket` to the address provided as `address`.

For non-local sockets `address` must be an [IP address object](#systemaddress-type--data--port--mode).
For local sockets `address` must be a string
(either a path on Unix or a pipe name on Windows).

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:connect ([address])`

Binds socket `socket` to the peer address provided as `address`,
thus any data send over the socket is targeted to that `address`.

For non-local domain sockets `address` must be an [IP address object](#systemaddress-type--data--port--mode).
For local domain sockets `address` must be a string
(either a path on Unix or a pipe name on Windows).

If `address` is not provided and `socket` is a datagram socket then it is unbinded from its previous binded peer address
(_i.e._ it is disconnected).
For stream sockets argument `address` is mandatory,
and it is necessary to bind the socket to a peer address before sending any data.
It is a [await function](#await) that awaits for the connection establishment on stream sockets.
This operation is not available for passive sockets.

Returns `true` in case of success.

### `socket:getaddress ([site [, address]])`

Returns the address associated with socket `socket`,
as indicated by `site`,
which can be:

- `"this"`: The socket's address (the default).
- `"peer"`: The socket's peer address.

For non-local domain sockets,
`address` can be an [IP address object](#systemaddress-type--data--port--mode) to store the result,
otherwise a new object is returned with the result data.

In case of errors,
it returns `nil` plus an error message.

### `socket:send (data [, i [, j [, address]]])`

[Await function](#await) that awaits until it sends through socket `socket` the substring of `data` that starts at `i` and continues until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

For unbinded datagram sockets `address` must be provided
(_i.e._ disconnected UDP sockets),
and it must be omitted for datagram sockets binded to a peer address
(_i.e._ connected UDP sockets).
For stream sockets `address` is ignored.
This operation is not available for passive sockets.

Returns `true` in case of success.

__Note__: if `data` is a [memory](https://github.com/renatomaia/lua-memory),
it is not converted to a Lua string prior to have its specified contents transfered.

### `socket:receive (buffer [, i [, j [, address]]])`

[Await function](#await) that awaits until it receives from socket `socket` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`,
following the same sematics of the arguments of [memory.get](https://github.com/renatomaia/lua-memory/blob/master/doc/manual.md#memoryget-m--i--j).

For datagram sockets,
if `address` is provided,
it is used to store the peer address that sent the data.
For stream sockets,
`address` is ignored.
This operation is not available for passive sockets.

Returns the number of bytes copied to `buffer`.
For datagram sockets,
it also returns a boolean indicating whether the copied data was truncated.

### `socket:joingroup (multicast [, interface])`

Joins network interface with address `interface` in the multicast group of address `multicast`,
thus datagrams targed to `multicast` address are delivered through address `interface`.
If `ifaceaddr` is not provided the socket `datagram` binded local address is used.

Both `multicast` and `interface` are string containing either textual (literal) or binary IP addresses
(see [`system.address`](#systemaddress-type--data--port--mode)).

This operation is only available for UDP sockets.

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:leavegroup (multicast [, interface])`

Removed network interface with address `interface` from the multicast group of address `multicast`
(see [`socket:joingroup`](#socketjoingroup-multicast--interface)).

This operation is only available for UDP sockets.

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:shutdown ()`

Shuts down the write side of stream socket `socket`.

This operation is only available for stream sockets.

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:listen (backlog)`

Starts listening for new connections on passive socket `socket`.
`backlog` is a hint for the underlying system about the suggested number of outstanding connections that shall be kept in the socket's listen queue.

This operation is only available for passive sockets.

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:accept ()`

[Await function](#await) that awaits until passive socket `socket` accepts a new connection.

This operation is only available for passive sockets.

Returns a new stream socket for the accepted connection.

### `system.load (chunk [, chunkname [, mode]])`

Returns a _system coroutine_ with [independent state](http://www.lua.org/manual/5.3/manual.html#lua_newstate) that executes in a separate system thread.
The code to be executed is given by the arguments `chunk`, `chunkname`, `mode`,
which are the same arguments of [`load`](http://www.lua.org/manual/5.3/manual.html#pdf-load).

The new _system coroutine_ has only the [`package`](http://www.lua.org/manual/5.3/manual.html#6.3) module loaded,
which provides function [`require`](http://www.lua.org/manual/5.3/manual.html#pdf-require).
This function can be called in the _system coroutine_ to load all other standard modules.
In particular, [basic functions](http://www.lua.org/manual/5.3/manual.html#6.1) can be loaded using `require "_G"`.

Moreover, the standard modules does not set global variables for their module tables.
To mimic the set up of the standard standalone interpreter use a code like below:

```lua
_G = require "_G"
package = require "package" -- loaded, but no global 'package'
coroutine = require "coroutine"
table = require "table"
io = require "io"
os = require "os"
string = require "string"
math = require "math"
utf8 = require "utf8"
debug = require "debug"
```

__Note__: These _system coroutines_ run in a separate thread,
but share the same [memory allocation](http://www.lua.org/manual/5.3/manual.html#lua_Alloc) and [panic function](http://www.lua.org/manual/5.3/manual.html#lua_atpanic) of the caller.
Therefore, it is required that thread-safe implementations are used,
such as the ones used in the [Lua standalone interpreter](http://www.lua.org/manual/5.3/manual.html#7).

### `system.loadfile ([filepath [, mode]])`

Similar to [`system.load`](#systemload-chunk--chunkname--mode), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.3/manual.html#pdf-loadfile).

### `syscoro:resume (...)`

[Await function](#await) that is like [`coroutine.resume`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume), but executes the [_system coroutine_](#systemload-chunk--chunkname--mode) `syscoro` on a separate system thread and awaits for its completion or [suspension](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield).
Moreover, only _nil_, _boolean_, _number_, _string_ and _light userdata_ values can be passed as arguments or returned from `syscoro`.

If the coroutine executing this [await function](#await) is explicitly resumed,
the execution of `syscoro` continues in the separate thread,
and it will not be able to be resumed again until it [suspends](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield).
In such case the results of the execution are discarted.

_System coroutines_ are executed using a limited set of threads that are also used by the underlying system.
The number of threads is given by environment variable [`UV_THREADPOOL_SIZE`](http://docs.libuv.org/en/v1.x/threadpool.html).

### `syscoro:status ()`

Like to [`coroutine.status`](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.resume),
but for [_system coroutines_](#systemload-chunk--chunkname--mode).

### `syscoro:close ()`

Like to [`coroutine.close`](http://www.lua.org/manual/5.4/manual.html#pdf-coroutine.close),
but for  [_system coroutines_](#systemload-chunk--chunkname--mode).

### `system.threads ([size])`

Returns a new _thread pool_ with `size` system threads to execute its [_tasks_](#threadsdostring-chunk--chunkname--mode-).

If `size` is omitted,
returns a new reference to the _thread pool_ where the calling code is executing,
or `nil` if it is not executing in a _thread pool_ (_e.g._ the main process thread).

### `threads:resize (size [, create])`

Defines that [_thread pool_](#systemthreads-size) `threads` shall keep `size` system threads to execute its [_tasks_](#threadsdostring-chunk--chunkname--mode-).

If `size` is smaller than the current number of threads,
the exceeding threads are destroyed at the rate they are released from the _tasks_ currently executing in `threads`.
Otherwise, new threads are created on demand until the defined value is reached.
Unless `create` evaluates to `true`,
in which case the new threads are created immediatelly.

### `threads:count (options)`

Returns numbers corresponding to the ammount of components in [_thread pool_](#systemthreads-size) `threads` according to the following characters present in string `options`:

- `n`: the total number of [_tasks_](#threadsdostring-chunk--chunkname--mode-).
- `r`: the number of _tasks_ currently executing.
- `p`: the number of _tasks_ pending to be executed.
- `s`: the number of _tasks_ not ready to execute.
- `e`: the expected number of system threads.
- `a`: the actual number of system threads.

### `threads:dostring (chunk [, chunkname [, mode, ...]])`

Loads a chunk as a new _task_ that executes in an independent state just like [_system coroutines_](#systemload-chunk--chunkname--mode).
However, such _tasks_ executes on system threads from [_thread pool_](#systemthreads-size) `threads`,
and starts as soon as a system thread is available.

Arguments `chunk`, `chunkname`, `mode` are the same of [`load`](http://www.lua.org/manual/5.3/manual.html#pdf-load).
Arguments `...` are passed to the loaded chunk,
but only _nil_, _boolean_, _number_, _string_ and _light userdata_ values are allowed as such arguments.

Whenever the loaded `chunk` [yields](http://www.lua.org/manual/5.3/manual.html#pdf-coroutine.yield) it reschedules itself as pending to be resumed,
and releases its running system thread.

Execution errors in the loaded `chunk` terminate the _task_,
and a message is written to the current process [standard error](https://en.wikipedia.org/wiki/Standard_streams#Standard_error_(stderr)) with prefix `[COUTIL PANIC]`.

Returns `true` if `chunk` is loaded successfully.

__Note__: A _task_ can yield a channel name followed by the arguments of [`channel:await`](#channelawait-endpoint-) to be suspended without the need to create a _channel_ nor coroutines.

### `threads:dofile (filepath [, mode, ...])`

Similar to [`threads:dostring`](#threadsdostring-chunk--chunkname--mode-), but gets the chunk from a file.
The arguments `filepath` and `mode` are the same of [`loadfile`](http://www.lua.org/manual/5.3/manual.html#pdf-loadfile).

### `threads:close ()`

When this function is called from a _task_ of [_thread pool_](#systemthreads-size) `threads`
(_i.e._ using a reference obtained by calling [`system.threads()`](#systemthreads-size) with no arguments),
it has no effect other than prevent further use of reference `threads`.

Otherwise, it waits until there are either no more _tasks_ or no more system threads,
and closes `threads` releasing all of its underlying resources.

Note that when _thread pool_ `threads` is garbage collected before this functions is called,
it will retain minimum resources until the termination of the Lua state they were created.
To avoid accumulative resource consumption by creation of multiple _thread pools_,
call this function on every _thread pool_.

Moreover, a _thread pool_ that is not closed will prevent the current Lua state to terminate (_i.e._ `lua_close` to return) until it has either no more tasks or no more system threads.

### `system.channelnames ([names])`

Returns a table mapping each name of existing channels to `true`.

If table `names` is provided,
returns `names`,
but first sets each of its string keys to either `true`,
if there is a channel with that name,
or `nil
`(removing it from `name`).

### `system.channel (name)`

Returns a new _channel_ with name `name`.

Channels with the same name share the same corresponding [_endpoints_](#channelawait-endpoint-).

### `channel:await (endpoint, ...)`

[Await function](#await) that awaits for a similar call on the opposite _endpoint_,
either from another coroutine, [_task_](#threadsdostring-chunk--chunkname--mode-) or [_system coroutine_](#systemload-chunk--chunkname--mode).

`endpoint` is a string that starts with either letter `"i"` or `"o"`,
each identifying an oposite _endpoint_.
If `endpoint` is not a string or starts with neither  `"i"` nor `"o"`,
this call will match with any call on either _endpoints_.

Returns `true` followed by the extra arguments `...` from the matching call.
Otherwise, return `false` followed by an error message.

### `channel:sync (endpoint, ...)`

Similar to [`channel:await`](#channelawait-endpoint-),
but does not await for a matching call.
In such case,
it returns `nil` followed by message "empty".

### `channel:close ()`

Closes channel `channel`.
Note that channels are automatically closed when they are garbage collected,
but that takes an unpredictable amount of time to happen. 

In case of success,
this function returns `true`.
Otherwise it returns `nil` plus an error message.

