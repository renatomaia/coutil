![logo](doc/logo.svg)

Coutil
======

Coutil provides a set of integrated libraries to support multithreading in Lua;
both cooperatively, using coroutines, and preemptively, by running code on distinct system threads.
It also provides synchronous (no callbacks) and non-blocking (suspends only the caller) functions for a variety of features:
from synchronization mechanisms like events, channels, and others;
to access to system resources like networking, processes, file system, and more.

**Note:** The access to system features is implemented over [`libuv`](https://libuv.org/),
thus it should be available in all plataforms supported by such library.

Documentation
-------------

- [License](LICENSE)
- [Install](doc/install.md)
- [Manual](doc/manual.md)
- [Development](doc/devnotes.md)

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

- File descriptor/handler polling suport.
