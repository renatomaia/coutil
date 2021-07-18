Demos
=====

These are usage examples of Coutil libraries.

## Feature Focused

These are examples designed to illustrate uses of particular features provided.

- Coroutine Finalizers
	- [Reporting uncaught/unexpected errors](spawn.lua)
- State Coroutines
	- [Ordinary functions as non-blocking](stateco.lua)
- Promises
	- [Awaiting for results out of order](promise.lua)
- File Information
	- [Showing information of a path](fileinfo.lua)
- System Information
	- [Showing system information](sysinfo.lua)

## Complete Applications

These are complete examples making use of multiple features to illustrate an application using Coutil.

- [Multithreaded Lua Console](console.lua): a Lua interpreter console with support for Coutil.
- [LuaSocket 2.0.2 API](luasocket.lua): a coroutine enabled version of module `socket.core`.
- [Multiprocess Echo Server](mutiecho): based on example from [libuv Guide](http://docs.libuv.org/en/v1.x/guide/processes.html#sending-file-descriptors-over-pipes).
- [TCP Server to Run Lua Chunks](remoteexec): includes server and client implentations,
either using Coutil,
or using callback-based library [luv](https://github.com/luvit/luv).

| Library | Client | Server |
| ------- | ------ | ------ |
| Coutil | [client.lua](remoteexec/client.lua) | [server.lua](remoteexec/server.lua) |
| luv | [client_cb.lua](remoteexec/client_cb.lua) | [server_cb.lua](remoteexec/server_cb.lua) |

## Performance Evaluation

These are implementations focused on evaluating the performance of some use cases comparing to similar uses with other libraries.

- Just yielding and being resumed:
[luv](idle/luv.lua),
[luv+coroutines](idle/luvcoro.lua),
[coutil](idle/coutil.lua),
[copas](idle/copas.lua).
- Uploading TCP stream:
[luv](tcp/upload/luv.lua),
[coutil](tcp/upload/coutil.lua),
[copas](tcp/upload/copas.lua).
- Sending and receiving TCP stream:
	- Each thread only starts receiving after sending, and vice-versa:
[luv](tcp/reqreply/serial/luv.lua),
[coutil](tcp/reqreply/serial/coutil.lua),
[copas](tcp/reqreply/serial/copas.lua).
	- Each thread sends and receives concurrently:
[luv](tcp/reqreply/multiplex/luv.lua),
[coutil](tcp/reqreply/multiplex/coutil.lua),
[copas](tcp/reqreply/multiplex/copas.lua).
