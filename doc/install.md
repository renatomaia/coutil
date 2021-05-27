Install
=======

To build it from the sources,
the following additional libraries are required.

- [libuv](https://github.com/libuv)
- [LuaMemory](https://github.com/renatomaia/lua-memory)

Once you have these libraries available,
you can build and install it as a rock from the sources using the provided [rockspec](etc/coutil-scm-1.rockspec):

```shell
luarocks make etc/coutil-scm-1.rockspec
```

If LuaRocks is not able to find the aforementioned libraries,
provide their location to LuaRocks as [external dependencies](https://github.com/luarocks/luarocks/wiki/Platform-agnostic-external-dependencies),
for instance:

```shell
luarocks make etc/coutil-scm-1.rockspec \
              LUAMEM_DIR=/usr/local/lua/5.4/memory/1.0 \
              LIBUV_DIR=/usr/local/libuv/1.41
```
