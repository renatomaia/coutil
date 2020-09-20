CoUtil Library
==============

`coutil` is a collection of Lua modules that provide utility functions for multithreading using coroutines. It provides support for:

- Coroutine execution finalizers.
- Coroutine syncronization abstractions:
	- [events](https://en.wikipedia.org/wiki/Async/await);
	- [mutexes](https://en.wikipedia.org/wiki/Mutex);
	- [promises](https://en.wikipedia.org/wiki/Futures_and_promises).
- Coroutine suspension until conditions on system features (and API to such features):
	- Measure of time lapse;
	- Signals to system processes;
	- Program execution;
	- Network and inter-process sockets;
	- DNS lookups;
	- Code chunk execution on separate system threads:
		- Preemptive coroutines;
		- Thread pools;
		- Channels to copy values between system threads.

Documentation
-------------

- [Manual](doc/manual.md)
- [License](LICENSE)

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

- Call 'lua_warn' whenever 'lua_resume' fails.
- Protect all 'lua_*' calls in 'uv_*' callbacks from raising errors (use 'pcall'?)
- Remove support to `getoption` on sockets and the like.
- User and group definition of process started with `system.execute`.
- Function to create an envionment variables set to be used in `system.execute`.
- Support `std{in,out,err}`in `system.execute` to be `"rw"` to create a pipe. See `UV_CREATE_PIPE`.
- Support for datagram UNIX domain sockets (not on Windows).
- Make `system.socket("stream", "pair")` return two pipe sockets created by POSIX `pipe` function.

### New Features

- TTY support.
- File system support (events and operations).
- File descriptor/handler polling suport.
