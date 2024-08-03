![logo](doc/logo.svg)

Coutil
======

The usefulness of Coutil library can be twofold:

### Multithreading in Lua

Coutil provides a set of Lua modules to support multithreading in Lua;
both cooperatively, using coroutines;
and preemptively, by running code on distinct system threads,
which can run in parallel making use of multiple CPU cores.
Coutil modules provide non-blocking (suspends only the caller) functions for a wide variety of features,
including synchronization mechanisms (events, channels, etc.) and much more.

### Portable Operating System API in Lua

Coutil modules include functions to access system resources like networking, processes, file system, and more.
Therefore,
even for single-thread applications,
Coutil can still be used simply as a library to access the features of modern operation systems in Lua.
In particular,
Coutil accesses the system features using [`libuv`](https://libuv.org/),
thus such support should be available in all platforms supported by such library.

For example,
consider we want to write a simple script that outpus a histogram of the [cryptographically strong random bytes](https://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator) produced by the operating system,
to validate that they are indeed uniformly distributed.
To do so,
we write the following script that creates a single coroutine
(non-blocking functions must run in a coroutine)
that calls Coutil's function [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) to get the random bytes from the operating system,
then calculates and outpus the histogram.

Documentation
-------------

- [License](LICENSE)
- [Install](doc/install.md)
- [Changes](doc/changelog.md)
- [Manual](doc/manual.md)
- [Demos](demo)
- [Contribute](doc/contributing.md)
- [Notes](doc/devnotes.md)
