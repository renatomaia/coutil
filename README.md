CoUtil Libraries
================

`coutil` provides a set of Lua libraries to support multithreading in Lua using coroutines,
in particular, to support:

- Coroutine execution finalizers.
- Coroutine syncronization abstractions:
	- [events](https://en.wikipedia.org/wiki/Async/await);
	- [mutexes](https://en.wikipedia.org/wiki/Mutex);
	- [promises](https://en.wikipedia.org/wiki/Futures_and_promises).
- Coroutine resumption on conditions on system features (and API to such features):
	- Measure of time lapse;
	- Signals of system processes;
	- Program execution;
	- Data transmission over network and IPC sockets;
	- DNS lookups;
	- Code chunk execution on separate system threads:
		- Preemptive coroutines;
		- Thread pools;
		- Copy values between code chunks.

**Note:** The support for system features is implemented over [`libuv`](https://libuv.org/) library,
therefore they should be available in all plataforms supported by such library.

Documentation
-------------

- [License](LICENSE)
- [Manual](doc/manual.md)
- [Source](doc/devnotes.md)

History
-------

### Version 2.1:
- Suspension of coroutines awaiting on system conditions provided by [`libuv`](https://libuv.org/) library, like time lapses, process signals, command executions, DNS lookups, data tranfers over socket, etc.
- Execution of code using preemptive system threads.
- Transmission of values between code executing on preemptive system threads.

### Version 2.0:
- `event.awaiteach` function removed.
- `await*` functions can be resumed using `coroutine.resume` and now return an additonal boolean indicating whether it was resumed by an event or not.

### Version 1.0:
- Creation of coroutines with finalizers.
- Syncronization abstractions for [cooperative multitasking](https://en.wikipedia.org/wiki/Cooperative_multitasking) using coroutines: [events](https://en.wikipedia.org/wiki/Async/await), [promises](https://en.wikipedia.org/wiki/Futures_and_promises) and [mutexes](https://en.wikipedia.org/wiki/Mutex).

TODO
----

### Improvements

- Protect all 'lua_*' calls in 'uv_*' callbacks from raising errors (use 'pcall'?)
- User and group definition of process started with `system.execute`.
- Replacement for package.cpath searcher that saves the 'luaopen_*' function to 'package.preload', so it can be shared by other threads (saves opened file descriptors).
- Support metamethod 'transfer' containing a _light userdata_ to a C function that "transfers" a userdata between independet Lua states.
	- Transferable coroutines that are closed on the source and move to the destiny.
	- Transferable memories that are resized to zero in the source and move to the destiny.
- Support for datagram UNIX domain sockets (not on Windows).
- Make `system.socket("stream", "pair")` return two pipe sockets created by POSIX `pipe` function.

### New Features

- TTY support.
- File system support (events and operations).
- File descriptor/handler polling suport.
