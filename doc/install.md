Index
=====

- [Dependencies](#dependencies)
- [UNIX](#unix)
- [Windows](#windows)
- [LuaRocks](#luarocks)

Contents
========

Dependencies
------------

To build it from the sources,
the following libraries are required.

- [Lua](https://www.lua.org)
- [LuaMemory](https://github.com/renatomaia/lua-memory)
- [libuv](https://github.com/libuv)

Some modules also depend during runtime of module [vararg](https://github.com/renatomaia/luavararg).
When installing [using LuaRocks](#luarocks) such runtime dependencies are automatically installed.

UNIX
----

Read the [`Makefile`](Makefile) for further details,
but you can usually build and install the Lua modules using the following commands:

```shell
make
make install
```

You can provide the location of your installation of Lua, LuaMemory, and libuv libraries using the following variables in the [`Makefile`](Makefile):

```shell
make LUA_DIR=/usr/local/lua/5.4 \
     LUAMEM_DIR=/usr/local/lua/5.4/memory/1.0 \
     LIBUV_DIR=/usr/local/libuv/1.41
make install INSTALL_DIR=/usr/local/lua/5.4/coutil/2.1
```

Windows
-------

Read the [`etc/Makefile.win`](etc/Makefile.win) for further details,
but you should be able to build and install the Lua modules using the `nmake` utility provided by Microsoft Visual C++.
For instance,
if your Lua and LuaMemory libraries are installed in `C:\Lua`,
and libuv is installed in `C:\libuv`,
you can type the following commands in a Microsoft Visual C++ console:

```shell
nmake /f etc/Makefile.win LUA_DIR=C:\Lua LIBUV_DIR=C:\libuv
nmake /f etc/Makefile.win install INSTALL_DIR=C:\Lua
```

LuaRocks
--------

You can install it as a rock from the sources using the provided [rockspec](etc/coutil-scm-1.rockspec):

```shell
luarocks make etc/coutil-scm-1.rockspec
```

Just make sure to provide to LuaRocks the LuaMemory and libuv libraries as available [external dependencies](https://github.com/luarocks/luarocks/wiki/Platform-agnostic-external-dependencies),
for instance:

```shell
luarocks make etc/coutil-scm-1.rockspec \
              LUAMEM_DIR=/usr/local/lua/5.4/memory/1.0 \
              LIBUV_DIR=/usr/local/libuv/1.41
```
