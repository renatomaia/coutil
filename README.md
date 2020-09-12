CoUtil Library
==============

`coutil` is a collection of modules that provide utility functions to support [cooperative multithreading](https://en.wikipedia.org/wiki/Cooperative_multitasking) using coroutines.

Documentation
-------------

- [Manual](doc/manual.md)
- [License](LICENSE)

History
-------

### Version 2.1:
- Schedule to resume coroutines using [`libuv`](https://libuv.org/).

### Version 2.0:
- `event.awaiteach` function removed.
- `await*` functions can be resumed using `coroutine.resume` and now return an additonal boolean indicating whether it was resumed by an event or not.

### Version 1.0:
- Abstractions for syncronization of coroutines: [events](https://en.wikipedia.org/wiki/Async/await), [promises](https://en.wikipedia.org/wiki/Futures_and_promises) and [mutexes](https://en.wikipedia.org/wiki/Mutex).
- Creation of coroutines with finalizers.

TODO
----

### Improvements

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
