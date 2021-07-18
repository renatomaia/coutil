![logo](doc/logo.svg)

Coutil
======

Coutil provides a set of integrated libraries to support multithreading in Lua;
both cooperatively, using coroutines;
and preemptively, by running code on distinct system threads.
It also provides synchronous (no callbacks) and non-blocking (suspends only the caller) functions for a variety of features:
from synchronization mechanisms like events, channels, and others;
to access to system resources like networking, processes, file system, and more.

**Note:** system features are implemented over [`libuv`](https://libuv.org/),
thus they should be available in all plataforms supported by such library.

Documentation
-------------

- [License](LICENSE)
- [Install](doc/install.md)
- [Changes](doc/changelog.md)
- [Manual](doc/manual.md)
- [Demos](demo)
- [Contribute](doc/contributing.md)
- [Notes](doc/devnotes.md)
