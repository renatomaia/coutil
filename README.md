CoUtil Library
==============

`coutil` is a Lua library to support syncronization of coroutines using events and promises.

Documentation
-------------

- [Manual](doc/manual.md)
- [License](LICENSE)

History
-------

### Version 2.0:
- `event.awaiteach` function removed.
- `await*` functions can be resumed using `coroutine.reume` and now return an additonal boolean indicating whether it was resumed by an event or not.

### Version 1.0:
- Abstractions for syncronization of coroutines: [events](https://en.wikipedia.org/wiki/Async/await), [promises](https://en.wikipedia.org/wiki/Futures_and_promises) and [mutexes](https://en.wikipedia.org/wiki/Mutex).
- Creation of coroutines with finalizers.
