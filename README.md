![logo](doc/logo.svg)

Coutil
======

Quick Links
-----------

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
It helps you mix a light form of multithreading using standard Lua coroutines with the use of system threads,
which allows you to do parallel computations in multi-core architectures.

### Portable API to the OS

Even if you do no care for multithreading,
Coutil can still be useful simply as a portable API to operating system resources,
like networking, processes, file system, and more.
Coutil provides functions that expose most of the features of [libuv](https://libuv.org/)
(a portable library used in the implementation of [Node.js](https://nodejs.org/)),
but hides its callback based API under conventional functions that return the results.

To illustrate this,
consider we want to use the operating system support to generate [cryptographically strong random bytes](https://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator),
and calculate a histogram of the values to validate that they are indeed uniformly distributed.
To do so,
we write the following script that calls Coutil's function [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) to get the random bytes from the operating system,
and calculates a histogram of its values.

```lua
local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
local buffer<const> = memory.create(8192)

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
all this can be done automatically by the [`demo/console.lua`](demo/console.lua) script that emulates the standard Lua console while doing all Coutil's boilerplate under the hood.
To execute the script `singlethread.lua` above,
use the `console.lua` script.

```
$ lua demo/console.lua singlethread.lua
................................................................................
....................
102593  102311  102265  102712  101846  102398  102741  102334
```

Although [`demo/console.lua`](demo/console.lua) implicitly created a coroutine to execute the script,
we did not made use of any multithreading explicitly,
and just used Coutil function [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) as any other module function.

The same way we call [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) in this example,
we can call any other Coutil function described in the [manual](doc/manual.md).
Check the [demos](demo) for usage examples of other Coutil functions.

If you execute the script above,
you might notice it can take quite some time to complete.
We can improve this by parallelizing it using Coutil's [thread pools](doc/manual.md#thread-pools).
To do so,
we first adapt the script above as the `worker.lua` script below,
which is intended to be executed by worker threads:

```lua
local channel = require "coutil.channel"

local buffer<const> = memory.create(8192)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")
local workerid<const> = ...

repeat
	assert(system.awaitch(startch, "in"))
	local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % #histogram)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(string.char(string.byte("A") + workerid - 1)); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram)))
until false
```

This script repeatedly awaits on [channel](doc/manual.md#channels) `startch` for a signal to calculate a partial histogram of random bytes just like we did in first script `singlethread.lua`.
Then it awaits on [channel](doc/manual.md#channels) `histoch` to publish the calculated partial histogram to an aggregator that shall sum all partial histograms to produce the final result.

Now,
we can write a second script that will start by creating the thread pool with tasks running the code from `worker.lua` script:

```lua
local threads = require "coutil.threads"

local ncpu<const> = #system.cpuinfo()
local pool<const> = threads.create(ncpu)
local console<const> = arg[-1]  -- path to the 'console.lua' script
local worker<const> = arg[0]:gsub("parallel%.lua$", "worker.lua")
for i = 1, ncpu do
	assert(pool:dofile(console, "t", "-W", worker, i))
end
```

Here we create a thread pool with one thread for each CPU core.
We use Coutil's function [`system.cpuinfo`](#systemcpuinfo-which) to get the number of CPU cores available in the system.
We use `arg` global variable from the console to get the path to the scripts.
Finally,
we start one worker task for each CPU core with [`threads:dofile`](doc/manual.md#threadsdofile-pool-filepath--mode-) method.
Here we also use the `console.lua` script to do the Coutil boilerplate for us when running the `worker.lua` script in the worker threads.
Moreover,
we provide the `-W` option to `console.lua` script to turn on warning,
since any uncaught errors in the code of pool thread tasks are always discarded,
and are only show as [warnings](http://www.lua.org/manual/5.4/manual.html#pdf-warn).

Before we start signaling the workers to generate histograms,
we start a separate thread in a coroutine using `spawn.call` function provided by the [`demo/console.lua`](demo/console.lua) script:

```lua
local channel = require "coutil.channel"

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

This coroutine awaits on a [channel](doc/manual.md#channels) for any partial histograms published by the workers on channels with name `"histogram"`,
and sums them to produce the final result.
The use of this another thread is important to process the results as soon as they are published,
independently from the code we will run in the main chunk.

Notice that after the coroutine sums all the 100 expected partial histograms,
it resizes the thread pool to remove all its threads.
This allows the tasks to be destroyed and the pool to be terminated.
Without this,
the script would hang indefinitely waiting for the tasks to terminate.

Finally,
we can start to signal the workers to produce as much histograms as we require by using a [channel](doc/manual.md#channels) with name `"start"`:

```lua
local startch<close> = channel.create("start")
for i = 1, repeats do
	io.write("."); io.flush()
	assert(system.awaitch(startch, "out"))
end
io.write("!"); io.flush()
```

If we put all the code above in a script named `parallel.lua`,
we can execute it using the `console.lua` script as in the example below:

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
- The `+` indicates the coroutine receiving and summing one of the partial histograms.
- The `!` indicates when the script's main chunk terminated.

In the output above,
we can see 12 workers (`A` to `L`) taking turns to process all the 100 `.` requested.
And also the coroutine aggregating the `+` results as soon as they are produced by the workers.

Notice how we mixed the use of two coroutines in the main thread with 12 worker threads.
Each worker thread could start additional threads in coroutines using `spawn.call`,
just like we did in the main thread.