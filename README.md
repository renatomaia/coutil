![logo](doc/logo.svg)

Coutil
======

- [License](LICENSE)
- [Install](doc/install.md)
- [Changes](doc/changelog.md)
- [Manual](doc/manual.md)
- [Demos](demo)
- [Contribute](doc/contributing.md)
- [Notes](doc/devnotes.md)

Overview
--------

There are two reasons Coutil may be appealing to Lua users:

### Multithreading Support

At its core,
Coutil is a library to support multithreading in Lua.
It helps you make use of a light form of multithreading using standard Lua coroutines,
and also use system threads that allow you to do parallel computations in multi-core architectures.

### Portable API to the OS

Even if you do no care for multithreading,
Coutil can still be useful simply as a portable API to operating system resources,
like [networking](doc/manual.md#network--ipc),
[processes](doc/manual.md#system-processes),
[file system](doc/manual.md#file-system),
and [more](doc/manual.md#summary).
Coutil provides functions that expose most of the features of [libuv](https://libuv.org/)
(a portable library used in the implementation of [Node.js](https://nodejs.org/)),
but hides its callback based API under conventional functions that take heavy inspiration from the functions in the standard library
(and libraries like [LuaSocket](https://lunarmodules.github.io/luasocket/),
[LuaFileSystem](https://github.com/lunarmodules/luafilesystem),
and [LuaProc](https://github.com/askyrme/luaproc) to name a few)
to provide a familiar Lua-like API to the OS.

To illustrate this,
consider we want to use the operating system support to generate [cryptographically strong random bytes](https://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator),
and calculate a histogram of the values to validate that they are indeed uniformly distributed.
To do so,
we write the following script that calls Coutil's function [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) to get the random bytes from the operating system,
and calculates a histogram of its values.

```lua
local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
local buffer<const> = memory.create(131072)

for i = 1, 100 do
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % #histogram)
		histogram[pos] = histogram[pos] + 1
	end
	io.write("."); io.flush()
end
print()
print(table.unpack(histogram))
```

Most Coutil functions like [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) require that you call them from a coroutine,
and also that you run Coutil's [event loop](doc/manual.md#event-processing).
Fortunately,
all this can be done automatically by the [`demo/console.lua`](demo/console.lua) script that emulates the [Lua standalone interpreter](https://www.lua.org/manual/5.4/manual.html#7) while doing all Coutil's boilerplate under the hood.
To execute the script `singlethread.lua` above,
use the `console.lua` script,
as shown below.

```
$ lua demo/console.lua singlethread.lua
................................................................................
....................
102593  102311  102265  102712  101846  102398  102741  102334
```

Although [`demo/console.lua`](demo/console.lua) implicitly creates a coroutine to execute the script,
we did not make use of any multithreading explicitly,
and just called [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) as any other function in a single-threaded script.
The same way we call [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) in this example,
we can call any other Coutil function described in the [manual](doc/manual.md).
Check the [demos](demo/README.md) for usage examples of other Coutil functions,
and also for a version of the [scripts in this page with the boilerplate](demo/randhist) required to be executed without the `console.lua` script.

If you execute the script above,
you might notice it can take quite some time to complete.
We can improve this by parallelizing it using Coutil's [thread pools](doc/manual.md#thread-pools).
To do so,
we first adapt the script above as the `worker.lua` script below,
which is intended to be executed by worker threads:

```lua
local i<const> = ...
local name<const> = string.char(string.byte("A") + i - 1)
local buffer<const> = memory.create(131072)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")

while true do
	assert(system.awaitch(startch, "in"))
	local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % #histogram)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(name); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram)))
end
```

This script repeatedly awaits on [channel](doc/manual.md#channels) `startch` for a signal to calculate a histogram of random bytes just like we did in first script `singlethread.lua`.
Then it awaits on [channel](doc/manual.md#channels) `histoch` to publish the calculated histogram to an aggregator that shall sum them to produce the final result.

Now,
we can write a second script that will start by creating the thread pool with [tasks](doc/manual.md#threadsdostring-pool-chunk--chunkname--mode-) running the code from `worker.lua` script:

```lua
local ncpu<const> = #system.cpuinfo()
local pool<const> = threads.create(ncpu)
local console<const> = arg[-1]  -- path to the 'console.lua' script
local worker<const> = arg[0]:gsub("parallel%.lua$", "worker.lua")
for i = 1, ncpu do
	assert(pool:dofile(console, "t", "-W", worker, i))
end
```

Here we create a thread pool with one thread for each CPU core.
We use Coutil's function [`system.cpuinfo`](doc/manual.md#systemcpuinfo-which) to get the number of CPU cores available in the system.
We use `arg` global variable from the console to get the path to the scripts.
Finally,
we start one worker task for each CPU core with [`threads:dofile`](doc/manual.md#threadsdofile-pool-filepath--mode-) method.
Here we also use the [`demo/console.lua`](demo/console.lua) script to do Coutil's boilerplate for us when running the `worker.lua` script in the thread pool.
Moreover,
we provide the `-W` option to `console.lua` script,
since any uncaught errors in the code of pool thread tasks are always discarded,
and are only shown as [warnings](http://www.lua.org/manual/5.4/manual.html#pdf-warn).

Before we start signaling the workers to generate histograms,
we start a separate thread in a coroutine using `spawn.call` function provided by the [`demo/console.lua`](demo/console.lua) script:

```lua
local repeats<const> = 100

spawn.call(function ()
	local histoch<close> = channel.create("histogram")
	local histogram<const> = {}
	for i = 1, repeats do
		local partial<const> = { select(2, assert(system.awaitch(histoch, "in"))) }
		for pos, count in ipairs(partial) do
			histogram[pos] = (histogram[pos] or 0) + count
		end
		io.write("+"); io.flush()
	end
	pool:resize(0)
	print()
	print(table.unpack(histogram))
end)
```

This coroutine awaits on a [channel](doc/manual.md#channels) for any histograms published by the workers,
and sums them to produce the final result.
The use of this other thread is important to process the results as soon as they are published,
independently from the code we will run in the main chunk.

Notice that after the coroutine sums all the 100 expected histograms,
it [resizes](doc/manual.md#threadsresize-pool-size--create) the thread pool to remove all its threads.
This allows the tasks to be destroyed and the pool to be terminated.
Without this,
the script would hang indefinitely waiting for the tasks to terminate.

Finally,
we can start to signal the workers to produce as much histograms as we require by using a [channel](doc/manual.md#channels):

```lua
local startch<close> = channel.create("start")
for i = 1, repeats do
	io.write("."); io.flush()
	assert(system.awaitch(startch, "out"))
end
io.write("!"); io.flush()
```

If we put all the code above in a script named `parallel.lua`,
we can execute it using the [`demo/console.lua`](demo/console.lua) script,
as shown below:

```
$ lua demo/console.lua parallel.lua
.............A+.F+.E+.C+.JG+B+.+..LI+DH++K++.....A+.F+.E+.C+.J+.B+.G+.L+.KI+D+..
H++..A+.F+.E+.B+.GC++..L+.J+.K+.I+.H+.D+.A+.F+.B+.E+.G+.C+.L+.J+.K+.I+.H+.D+.A+.
F+.B+.E+.C+.G+.L+.J+.I+.K+.D+.A+.H+.F+.B+.G+.E+.C+.L+.J+.I+.AK++..D+.H+.F+.G+.E+
.B+.C+.L+.J+.A+.I+.K+.B+.F+.G+.D.+E+!H+C+A+J+I+L+F+B+D+K+G+E+
102497  102455  102406  102253  102306  102212  102382  102689
```

If you run this on a multi-core architecture,
you will notice it executes faster than its previous single-thread version.
It will also output a sequence of characters that illustrates roughly the order the tasks and coroutines executed:
- The `.` indicates the script's main chunk requesting a histogram from a worker.
- The letters indicates one of the workers publishing its results.
- The `+` indicates the coroutine receiving and summing one of the histograms.
- The `!` indicates when the script's main chunk terminated.

In the output above,
we can see 12 workers (`A` to `L`) taking turns to process all the 100 `.`.
And also the coroutine aggregating the published results `+` as soon as they are produced by the workers.

Notice how we mixed the use of two coroutine-based threads in the main thread with 12 tasks in other threads.
Each task can start their own additional coroutine-based threads using `spawn.call`,
just like we did in the main thread.

Coroutine-based threads execute concurrently and sharing the same data inside a single system thread,
while tasks can run in parallel in separate system threads,
but only exchange data through [channels](doc/manual.md#channels) and [IPC mechanisms](doc/manual.md#network--ipc).
We can also start new [processes](doc/manual.md#system-processes) for more isolation,
which can only communicate through [IPC mechanisms](doc/manual.md#network--ipc).
Coutil is designed to allow us to mix all these mechanisms when composing our applications.
