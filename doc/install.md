Install
=======

You can install Coutil using [LuaRocks](https://www.luarocks.org/):

```shell
luarocks install coutil --server=https://luarocks.org/manifests/maia
```

If LuaRocks is not able to find library [libuv](https://libuv.org) (version 1.x),
provide the location of the missing external dependency as described [here](https://github.com/luarocks/luarocks/wiki/Platform-agnostic-external-dependencies),
for instance:

```shell
luarocks install coutil --server=https://luarocks.org/manifests/maia \
                        LIBUV_DIR=/usr/local/libuv/1.41
```

Build from Source
=================

All the commands in the following sections are expected to be executed from the root of this Git repository.

LuaRocks
--------

You can use [LuaRocks](https://luarocks.org) to build and install it as a rock from the sources using the provided [rockspec](../etc/coutil-scm-1.rockspec):

```shell
luarocks make etc/coutil-scm-1.rockspec
```

CMake
-----

You also can build the binary modules with [CMake](https://cmake.org/)
(check the [rockspec](../etc/coutil-scm-1.rockspec) for the files that implement the script modules):

```shell
cmake -B build -S . -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON
cmake --build build --config Release
cmake --install build
```

**Note:** You can add `-DLINK_TO_LUAMEM_LIB=OFF` to the first command to avoid linking Coutil's binary module to LuaMemory's C library.
This might be useful to make Coutil use a LuaMemory implementation from the Lua runtime.
For instance,
when [Lua is built with LuaMemory](https://github.com/renatomaia/coutil/blob/master/doc/install.md#added-to-lua),
or [LuaMemory's library loaded in Lua is exposed to modules](https://github.com/renatomaia/coutil/blob/master/doc/install.md#expose-the-c-api-dynamically).
