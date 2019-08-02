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
- `await*` functions can be resumed using `coroutine.reume` and now return an additonal boolean indicating whether it was resumed by an event or not.

### Version 1.0:
- Abstractions for syncronization of coroutines: [events](https://en.wikipedia.org/wiki/Async/await), [promises](https://en.wikipedia.org/wiki/Futures_and_promises) and [mutexes](https://en.wikipedia.org/wiki/Mutex).
- Creation of coroutines with finalizers.

TODO
----

### Improvements

- `permission` option for local (Pipe) sockets.
- Test all yield cases for operation `system.execute`.
- Use of streams (TCP, Pipe, etc.) as process I/O (`stdout`, `stderr`).
- User and group definition of process started with `system.execute`.
- Function to create an envionment variables set to be used in `system.execute`.

### New Features

- TTY support.
- File system support (events and operations).
- Threading support.
- File descriptor/handler polling suport.
