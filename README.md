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
Coutil is a library to support multithreading in Lua:
it helps you mix a lighter form of multithreading using standard Lua coroutines;
with also the use of system threads,
which allows you to do parallel computations in multi-core architectures for instance.

### Portable API to the OS

Even if you do no care for multithreading,
Coutil can still be very useful simply as a portable API to operating system features.
Coutil provides functions to manipulate sockets, processes, the file system, and much more.
Coutil exposes most of the features of [libuv](https://libuv.org/) (a portable library used in the implementation of [Node.js](https://nodejs.org/)),
but hides its callback based API under convential functions that return the results when done.

### Illustrative Examples

To illustrate this,
consider we want to use the operating system support to generate [cryptographically strong random bytes](https://en.wikipedia.org/wiki/Cryptographically_secure_pseudorandom_number_generator),
and calculate a histogram of the values to validate that they are indeed uniformly distributed.
To do so,
we write the following script that calls Coutil's function [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) to get the random bytes from the operating system,
and calculates a histogram of its values.

```lua
local repeats<const> = 100
local histlen<const> = 8
local buffer<const> = memory.create(8192)

local histogram = setmetatable({}, {__index = function () return 0 end})
for i = 1, 100 do
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % histlen)
		histogram[pos] = histogram[pos] + 1
	end
end
print(table.unpack(histogram, 1, histlen))
```

Most Coutil functions like [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) require that you call them from a coroutine,
and also that you run its event loop.
Fortunately,
all this boilerplate can be done automatically by the [`demo/console.lua`](demo/console.lua) script that emulates the standard Lua console.
To execute the script `singlethread.lua` above,
use the `console.lua` script:

```
$ lua demo/console.lua singlethread.lua
102593  102311  102265  102712  101846  102398  102741  102334
```

The same way we call [`system.random`](doc/manual.md#systemrandom-buffer--i--j--mode) in this example,
we can call any other Coutil functions,
which can for instance,
read from a socket,
start a new process,
and much more.

If you execute the script above,
you might notice it can take some time to execute.
We can improve this by paralellizing it using Coutil's [thread pools](#thread-pools).
First,
we adapt script `singlethread.lua` above as the `worker.lua` script below to be executed by worker threads:

```lua
local channel = require "coutil.channel"

local id<const> = ...
local histlen<const> = 8
local buffer<const> = memory.create(8192)
local startch<close> = channel.create("start")
local histoch<close> = channel.create("histogram")

repeat
	assert(system.awaitch(startch, "in"))
	local histogram<const> = setmetatable({}, {__index = function () return 0 end})
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % histlen)
		histogram[pos] = histogram[pos] + 1
	end
	io.write(string.char(64+id)); io.flush()
	assert(system.awaitch(histoch, "out", table.unpack(histogram, 1, histlen)))
until false
```

In this script we initially await on the `startch` channel (with name `"start"`) for a message that will signal the worker shall calculate a partial histogram of random bytes just like we did in first script `singlethread.lua`.
Then it publishes the calculated partial histogram through `histoch` channel (with name `"histogram"`) to an aggregator that will sum all partial historgrams to produce the final result.

Now,
we can write a second script that will first create a thread pool with tasks running the code from `worker.lua` script,
as illustrated below:

```lua
local threads = require "coutil.threads"

local ncpu<const> = #system.cpuinfo()
local pool<const> = threads.create(ncpu)
local console<const> = arg[-1]
local worker<const> = arg[0]:gsub("parallel%.lua$", "worker.lua")
for i = 1, ncpu do
	assert(pool:dofile(console, "t", worker, i))
end
```

Here we create a thread pool with one thread for each CPU core.
We use Coutil's function [`system.cpuinfo`](#systemcpuinfo-which) to get the number of CPU core available in the system.
We use `arg` global variable from the console to get the path to the scripts.
Finally,
we create one worker task for each CPU core.
Here we also use the `console.lua` script to do the Coutil boilerplate for us.

Before we start sending requests to the workers,
we start a separate coroutine using `spawn.call` function provided by the [`demo/console.lua`](demo/console.lua) script.
This separate coroutine will sum the partial histograms published by the workers on channel with name `"histogram"`,
as illustrated below

```lua
local vararg = require "vararg"
local channel = require "coutil.channel"

local repeats<const> = 100

spawn.call(function ()
	local histoch<close> = channel.create("histogram")
	local histogram<const> = setmetatable({}, {__index = function () return 0 end})
	for i = 1, repeats do
		local partial = vararg.pack(select(2, assert(system.awaitch(histoch, "in"))))
		io.write("+"); io.flush()
		for pos, count in partial do
			histogram[pos] = histogram[pos] + count
		end
	end
	pool:resize(0)
	print()
	print(table.unpack(histogram, 1, histlen))
end)
```

Notice that after the coroutine sums all the partial histograms,
it resizes the thread pool to remove all its threads.
This allows its tasks to be destroyed and the pool to be terminated.
Without this,
the script would hang indenitely waiting for the tasks to terminate.

Finally,
we can publish messages on the channel with name `"start"` to request the workers to produce as much histograms as we require.

```lua
local startch<close> = channel.create("start")
for i = 1, repeats do
	io.write("*"); io.flush()
	assert(system.awaitch(startch, "out"))
end
io.write("."); io.flush()
```

If we put all these code chunks above in a script named `parallel.lua`,
we can execute it using the `console.lua` script as illustrated below:

```
$ lua demo/console.lua parallel.lua
*************C+*B+*A+*L+*D+*E+*K+*H+*G+I*+*J+*F+*C+*A+*L+*B+*D+*E+*K+*H+*I+*G+*J
+F*+*L+*A+*C+*B+*E+*D+*H+*K+*I+*G+*J+*F+*L+*C+*A+*B+*E+*H+D*+*K+*G+*I+*J+*L+*C+*
A+*F+*B+*E+*H+*K+*D+*G+*I+*C+*L+*J+*A+*E+*H+*K+*F+*D+*G+*I+*L+*A+*JC++**H+*B+*E+
*D+*F+*G+*K+*I+*L+*C+*A+*H+*B+*G+*D+.J+F+E+K+I+A+H+L+C+G+D+B+
101576  102759  102589  101983  102919  102543  102263  102568
```

You will notice it executes much faster than its previous single-thread version.
It will also output a sequence of characters that illustrates roughly the order the tasks and coroutines executed:
- The `*` indicates the script's main chunk requesting a histogram from a worker.
- The letters indicates one of the workers publishing its results.
- The `+` indicates the coroutine receiving and summing one of the partial histograms.
- The `.` indicates when the script's main chunk terminated.

In the output above,
we can see 12 workers (`A` to `L`) taking turns to process all the 100 `*` requested.
And also the coroutine aggregating the `+` results as soon as they are produced by the workers.