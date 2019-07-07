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
	- [`system.halt`](#systemhalt-)
	- [`system.pause`](#systempause-delay)
	- [`system.awaitsig`](#systemawaitsig-signal)
	- [`system.execute`](#systemexecute-cmd-)
	- [`system.address`](#systemaddress-type--data--port--mode)
	- [`system.findaddr`](#systemfindaddr-name--service--mode)
	- [`system.socket`](#systemsocket-type--domain)
		- [`socket:close`](#socketclose-)
		- [`socket:getdomain`](#socketgetdomain-)
		- [`socket:bind`](#socketbind-address)
		- [`socket:connect`](#socketconnect-address)
		- [`socket:getaddress`](#socketgetaddress-site--address)
		- [`socket:setoption`](#socketsetoption-name-value)
		- [`socket:getoption`](#socketgetoption-name)
		- [`socket:send`](#socketsend-data--i--j--address)
		- [`socket:receive`](#socketreceive-buffer--i--j--address)
		- [`datagram:joingroup`](#datagramjoingroup-multicast--interface)
		- [`datagram:leavegroup`](#datagramleavegroup-multicast--interface)
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

### `system.halt ()`

Causes [`system.run`](#systemrun-mode) to return prematurely.

### `system.pause ([delay])`

Suspends the execution of the calling coroutine, and schedules it to be resumed after `delay` seconds have passed since the coroutine was last resumed.

If `delay` is not provided or is `nil`, the coroutine is scheduled as ready, so it will be resumed as soon as possible.
The same is applies when `delay` is zero or negative.

`pause` returns `true` if the calling coroutine is resumed as scheduled.
Otherwise it returns like [`event.await`](#eventawait-e).
In any case, the coroutine is not scheduled to be resumed anymore after it returns.

### `system.awaitsig (signal)`

Suspends the execution of the calling coroutine,
and schedules it to be resumed when the process receives signal indicated by string `signal`, as listed below:

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

### `system.execute (cmd, ...)`

Executes a new process,
and suspends the execution of the calling coroutine until it terminates.
`cmd` is the path of the executable image for the new process.
Every other extra arguments are strings to be used as command-line arguments for the executable image of the new process.

Alternatively, `cmd` can be a table with the fields described below.
Unless stated otherwise, when one of these field are not defined in table `cmd`, or `cmd` is a string, the new process inherits the characteristics of the current process, like the current directory, the environment variables, or standard files.

- `execfile`:
path of the executable image for the new process.
This field is required.

- `runpath`:
path of the current directory of the new process.

- `stdin`:
file to be set as the standard input of the new process.

- `stdout`:
file to be set as the standard output of the new process.

- `stderr`:
file to be set as the standard error output of the new process.

- `arguments`:
table with the sequence of command-line arguments for the executable image of the new process.
When this field is not provided, the new process's executable image receives no arguments.

- `environment`:
table mapping environment variable names to the values they must assume for the new process.
If this field is provided, only the variables defined will be available for the new process.

If `cmd` is a table,
the field `pid` is set with a number that identifies the new process
(_e.g._ UNIX process identifier)
before the calling coroutine is suspended.

Returns a string when the new process terminates,
or `nil` plus an error message otherwise.
The returned string is followed by extra values, as follows:

- `"exit"`: the process terminated normally;
it is followed by a number of the exit status of the program.

- `"signal"`: the process was terminated by a signal;
it is followed by a string with the name of the signal that terminated the program,
and the number of the signal.
The strings that represent signals are described in [`system.awaitsig`](#systemawaitsig-signal).

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

### `system.findaddr (name [, service [, mode]])`

Searches for the addresses of network name `name`,
and suspends the execution of the calling coroutine until it concludes.
If `name` is `nil`, the loopback address is searched.
If `name` is `"*"`, the wildcard address is searched.

`service` indicates the port number or service name to be used to resolve the port number of the resulting addresses.
When `service` is absent, the port zero is used in the results.
The string `mode` defines the search domain. 
It can contain any of the following characters:

- `4`: for IPv4 addresses.
- `6`: for IPv6 addresses.
- `m`: for IPv4-mapped addresses.
- `d`: for addresses for `datagram` sockets.
- `s`: for addresses for `stream` or `listen` sockets.

When neither `4` nor `6` are provided, the search only includes addresses of the same type configured in the local machine.
When neither `d` nor `s` are provided, the search behaves as if both `d` and `s` were provided.
By default, `mode` is the empty string.

Returns `nil` plus an error message in case of errors.
Otherwise, returns an iterator that have the following usage pattern:

	[address, socktype, nextdomain =] getnext ([address])

Each time the iterator is called, returns one address found for node with `name`,
followed by the type of the socket to be used to connect to the address,
and the type of the next address.
If an address structure is provided as `address`, it is used to store the result;
otherwise a new address structure is created.
Therefore, `nextdomain` is `"ipv4"` if the next address id a IPv4 address, or `"ipv6"` if the next address id a IPv6 address, or `nil` if the next call will return no address.

As an example, the following loop will iterate over all the addresses found for service named 'ssh' on node named `www.lua.org`, using the same IPv4 address object:

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

Finally, an example that fills existing addreses objects with the results

```lua
address = { ipv4 = system.address("ipv4"), ipv6 = system.address("ipv6") }
getnext, nextdomain = assert(system.findaddr("www.lua.org", "http", "s"))
repeat
	addr, scktype, nextdomain = getnext(address[nextdomain])
until nextdomain == nil
```

### `system.socket (type [, domain])`

Creates a socket, of the type specified by `type`,
which is either:

- `"datagram"` creates UDP datagram socket for data transfers.
- `"stream"` creates TCP stream socket for data transfers.
- `"listen"` creates TCP listen socket to accept connected TCP stream sockets.

The `domain` string defines the socket's address domain (or family),
and can be  either `"ipv4"` or `"ipv6"`,
to create socket on the IPv4 or IPv6 address domain.

On success it returns a new socket,
or `nil` plus an error message otherwise.

### `socket:close ()`

Closes socket `socket`.
Note that sockets are automatically closed when their handles are garbage collected,
but that takes an unpredictable amount of time to happen. 

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:getdomain ()`

Returns the address domain of `socket`, which can be either `"ipv4"` or `"ipv6"`.

### `socket:bind (address)`

Binds socket `socket` to the local address provided as `address`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:connect ([address])`

Binds socket `socket` to the peer address provided as `address`,
thus any data send over the socket is targeted to that `address`.

If `address` is not provided and `socket` is a UDP socket then it is unbinded from its previous binded peer address
(_i.e._ it is disconnected).
For TCP stream sockets argument `address` is mandatory,
and it is necessary to bind the socket to a peer address before sending any data.
This operation is not available for sockets of type `listen`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:getaddress ([site [, address]])`

Returns the address associated with socket `socket`, as indicated by `site`, which can be:

- `"this"`: The socket's address (the default).
- `"peer"`: The socket's peer address.

If `address` is provided, it is the address structure used to store the result,
otherwise a new `address` object is returned with the result data.

In case of errors, it returns `nil` plus an error message.

### `socket:setoption (name, value)`

Sets `value` as the value of option `name` for socket `socket`.
This operation is not available for sockets of type `listen`.
The available options are the same as defined in operation [`socket:getoption`](#socketgetoption-name).

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `socket:getoption (name)`

Returns the value of option `name` of socket `socket`.
This operation is not available for sockets of type `listen`.
There available options are:

#### `datagram` Socket Options

- `"broadcast"`: is `true` when broadcast is enabled in `socket`,
or `false` otherwise.
- `"mcastloop"`: is `true` when loopback of outgoing multicast datagrams is enabled,
or `false` otherwise.
(default is `true`).
- `"mcastttl"`: is a number from 1 to 255 of multicast time to live.
(default is 1).
- `"mcastiface"`: is the address of the interface for multicast,
or `nil` is none is defined.

#### `stream` Socket Options

- `"keepalive"`: is a number of seconds of the initial delay of the periodic transmission of messages when the TCP keep-alive option is enabled,
or `nil` otherwise.
- `"nodelay"`: is `true` when coalescing of small segments shall be avoided,
or `false` otherwise.

### `socket:send (data [, i [, j [, address]]])`

Sends the substring of `data` that starts at `i` and continues until `j`;
`i` and `j` can be negative.
If `j` is absent,
it is assumed to be equal to -1 (which is the same as the string length).

For unbinded sockets of type `datagram` `address` must be provided
(_i.e._ disconnected UDP sockets),
and it must be omitted for sockets of type `datagram` binded to a peer address
(_i.e._ connected UDP sockets).
For sockets of type `stream` `address` is ignored.
This operation is not available for sockets of type `listen`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

__Note__: if `data` is a [memory](https://github.com/renatomaia/lua-memory), it is not converted to a Lua string prior to have its specified contents transfered.

### `socket:receive (buffer [, i [, j [, address]]])`

Receives from socket `socket` at most the number of bytes necessary to fill [memory](https://github.com/renatomaia/lua-memory) `buffer` from position `i` until `j`;
`i` and `j` can be negative.
If `j` is absent, it is assumed to be equal to -1
(which is the same as the buffer size).

For sockets of type `datagram`,
if `address` is provided,
it is used to store the peer address that sent the data.
For sockets of type `stream`,
`address` is ignored.
This operation is not available for sockets of type `listen`.

In case of success,
returns the number of bytes copied to `buffer` and,
for sockets of type `datagram`,
a boolean indicating whether the copied data was truncated.
Otherwise it returns `nil` plus an error message.

### `datagram:joingroup (multicast [, interface])`

Joins network interface with address `interface` in the multicast group of address `multicast`,
thus datagrams targed to `multicast` address are delivered through address `interface`.
If `ifaceaddr` is not provided the socket `datagram` binded local address is used.

Both `multicast` and `interface` are string containing either textual (literal) or binary IP addresses
(see [`system.address`](#systemaddress-type--data--port--mode)).

This operation is only available for sockets of type `datagram`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `datagram:leavegroup (multicast [, interface])`

Removed network interface with address `interface` from the multicast group of address `multicast`
(see [`datagram:joingroup`](#datagramjoingroup-multicast--interface)).

This operation is only available for sockets of type `datagram`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `stream:shutdown ()`

Shuts down the write side of TCP stream socket `stream`.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `listen:listen (backlog)`

Starts listening for new connections on TCP listen socket `listen`.
`backlog` is a hint for the underlying system about the suggested number of outstanding connections that shall be kept in the socket's listen queue.

In case of success, this function returns `true`.
Otherwise it returns `nil` plus an error message.

### `listen:accept ()`

Accepts a new pending connection on TCP listen socket `listen`.

Returns a new `stream` socket for the accepted connection in case of success.
Otherwise it returns `nil` plus an error message.
