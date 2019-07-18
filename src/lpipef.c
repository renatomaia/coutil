#include "looplib.h"
#include "lmodaux.h"
#include "loperaux.h"
#include "lnetwaux.h"

#include <string.h>
#include <lmemlib.h>


static size_t posrelat (ptrdiff_t pos, size_t len) {
	if (pos >= 0) return (size_t)pos;
	else if (0u - (size_t)pos > len) return 0;
	else return len - ((size_t)-pos) + 1;
}

static int getbufarg (lua_State *L, uv_buf_t *buf) {
	size_t sz;
	const char *data = luamem_checkstring(L, 2, &sz);
	if (data) {
		size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
		size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
		if (start < 1) start = 1;
		if (end > sz) end = sz;
		buf->len = end-start+1;
		buf->base = (char *)(data+start-1);
		return 1;
	}
	return 0;
}

static void completereqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, loop, request);
	}
}


/*
 * PIPE
 */

#define defobjectaux(NAME, TYPE) \
	static lcu_##TYPE *opened##NAME (lua_State *L) { \
		lcu_##TYPE *object = lcu_check##NAME(L,1); \
		luaL_argcheck(L, !lcu_is##NAME##closed(object), 1, "closed "#NAME); \
		return object; \
	} \
	 \
	static lcu_##TYPE *owned##NAME (lua_State *L, uv_loop_t *loop) { \
		lcu_##TYPE *object = opened##NAME(L); \
		luaL_argcheck(L, lcu_to##NAME##handle(object)->loop == loop, 1, \
			               "foreign object"); \
		return object; \
	} \
	 \
	static int NAME##_tostring (lua_State *L) { \
		lcu_##TYPE *object = lcu_check##NAME(L,1); \
		if (!lcu_is##NAME##closed(object)) lua_pushfstring(L, #NAME" (%p)", pipe); \
		else lua_pushliteral(L, #NAME" (closed)"); \
		return 1; \
	} \
	 \
	static int NAME##_gc (lua_State *L) { \
		lcu_close##NAME(L, 1); \
		return 0; \
	} \
	 \
	static int NAME##_close (lua_State *L) { \
		lcu_##TYPE *object = lcu_check##NAME(L, 1); \
		if (!lcu_is##NAME##closed(pipe)) { \
			int closed = lcu_close##NAME(L, 1); \
			lcu_assert(closed); \
			lua_pushboolean(L, closed); \
		} \
		else lua_pushboolean(L, 0); \
		return 1; \
	}

defobjectaux(pipe, IpcPipe);


/* pipe [, errmsg] = system.pipe(ipc) */
static int system_pipe (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	int ipc = luaL_toboolean(L, 1);
	lcu_IpcPipe *pipe = lcu_newpipe(L);
	int err = uv_pipe_init(loop, lcu_topipehandle(pipe), ipc);
	if (!err) lcu_enablepipe(pipe);
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = pipe:bind(address) */
static int pipe_bind (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L);
	const char *addr = luaL_checkstring(L, 2);
	int err = uv_pipe_bind(lcu_topipehandle(pipe), addr);
	return lcuL_pushresults(L, 0, err);
}


/* address [, errmsg] = pipe:getaddress([site]) */
static int pipe_getaddress (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L);
	uv_pipe_t *handle = lcu_topipehandle(pipe);
	int peer = luaL_checkoption(L, 2, "this", sites);
	char mem[LCU_PIPEADDRBUF];
	size_t bufsz = LCU_PIPEADDRBUF;
	char *buf = mem;
	int err;
again:
	if (peer) err = uv_pipe_getpeername(handle, buf, &bufsz);
	else err = uv_pipe_getsockname(handle, buf, &bufsz);
	if (!err) {
		lua_pushlstring(L, buf, bufsz);
	} else if (err == UV_ENOBUFS && buf == mem) {
		buf = (char *)lua_newuserdata(L, bufsz);
		goto again;
	}
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = pipe:setperm(options) */
static int set_setperm (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L);
	uv_pipe_t *handle = lcu_topipehandle(pipe);
	const char *mode = luaL_optstring(L, 3, "");
	int err;
	for (; *mode; ++mode) switch (*mode) {
		case 'r': flags != UV_READABLE; break;
		case 'w': flags != UV_WRITABLE; break;
		default: return luaL_error(L, "unknown option (got "LUA_QL("%c")")", *mode);
	}
	err = uv_pipe_chmod(handle, flags);
	return lcuL_pushresults(L, 0, err);
}


/* succ [, errmsg] = pipe:connect(address) */
static int k_setuppipeconn (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_IpcPipe *pipe = ownedpipe(L, loop);
	const char *addr = luaL_checkstring(L, 2);
	uv_connect_t *connect = (uv_connect_t *)request;
	return uv_pipe_connect(connect, lcu_topipehandle(pipe), addr, uv_onconnected);
}
static int pipe_connect (lua_State *L) {
	return lcuT_resetreqopk(L, k_setuppipeconn, NULL);
}


/* succ [, errmsg] = socket:listen(backlog) */
static void uv_onconnection (uv_stream_t *stream, int status) {
	lua_State *thread = (lua_State *)stream->data;
	if (thread) {
		lua_pushinteger(thread, status);
		lcuU_resumeobjop(thread, (uv_handle_t *)stream);
		if (stream->data == NULL) uv_unref((uv_handle_t *)stream);
		else lcu_assert(uv_has_ref((uv_handle_t *)stream));
	}
	else lcu_addpipelisten((lcu_IpcPipe *)stream);
}
static int pipe_listen (lua_State *L) {
	lcu_IpcPipe *pipe = ownedpipe(L, lcu_toloop(L));
	uv_stream_t *stream = (uv_stream_t *)lcu_topipehandle(pipe);
	lua_Integer backlog = luaL_checkinteger(L, 2);
	int err;
	luaL_argcheck(L, !lcu_ispipelisten(pipe), 1, "already listening");
	luaL_argcheck(L, 0 <= backlog && backlog <= INT_MAX, 2, "large backlog");
	err = uv_listen(stream, (int)backlog, uv_onconnection);
	if (err >= 0) {
		lcu_markpipelisten(pipe);
		uv_unref((uv_handle_t *)stream); /* uv_listen_stop */
	}
	return lcuL_pushresults(L, 0, err);
}


/* pipe [, errmsg] = socket:accept() */
static int k_acceptpipe (lua_State *L, int status, lua_KContext ctx) {
	lcu_IpcPipe *pipe = openedpipe(L);
	uv_stream_t *stream = (uv_stream_t *)lcu_topipehandle(pipe);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcuT_haltedobjop(L, (uv_handle_t *)stream)) {
		int domain = lcu_getpipeaddrfam(pipe);
		lcu_IpcPipe *newpipe = lcu_newpipe(L, domain, LCU_TCPTYPE_STREAM);
		uv_pipe_t *newhdl = lcu_topipehandle(newpipe);
		int err = uv_pipe_init(stream->loop, newhdl);
		if (err >= 0) {
			err = uv_accept(stream, (uv_stream_t *)newhdl);
			if (err >= 0) lcu_enablepipe(newpipe);
		}
		return lcuL_pushresults(L, 1, err);
	}
	uv_unref((uv_handle_t *)stream);  /* uv_listen_stop */
	return lua_gettop(L)-1;
}
static int pipe_accept (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_IpcPipe *pipe = ownedpipe(L, loop);
	luaL_argcheck(L, lcu_ispipelisten(pipe), 1, "not listening");
	if (!lcu_pickpipelisten(pipe)) {
		uv_handle_t *handle = (uv_handle_t *)lcu_topipehandle(pipe);
		luaL_argcheck(L, handle->data == NULL, 1, "already used");
		if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
		uv_ref(handle);  /* uv_listen_start */
		lcuT_awaitobj(L, handle);
		return lua_yieldk(L, 0, 0, k_acceptpipe);
	}
	lua_pushlightuserdata(L, loop);  /* token to sign scheduled */
	return k_acceptpipe(L, LUA_YIELD, 0);
}


/* succ [, errmsg] = pipe:shutdown() */
static int k_setuppipeshut (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_IpcPipe *pipe = ownedpipe(L, loop, LCU_PIPETYPE_STREAM);
	uv_shutdown_t *shutdown = (uv_shutdown_t *)request;
	uv_stream_t *stream = (uv_stream_t *)lcu_topipehandle(pipe);
	return uv_shutdown(shutdown, stream, uv_onshutdown);
}
static int pipe_shutdown (lua_State *L) {
	return lcuT_resetreqopk(L, k_setuppipeshut, NULL);
}














/* sent [, errmsg] = pipe:send(data [, i [, j [, pipe]]]) */
static void uv_onsent (uv_pipe_send_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupsend (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_IpcPipe *pipe = ownedpipe(L, loop);
	uv_pipe_send_t *send = (uv_pipe_send_t *)request;
	uv_pipe_t *handle = (uv_pipe_t *)lcu_topipehandle(pipe);
	uv_buf_t bufs[1];
	const struct sockaddr *addr = lcu_getpipeconnected(pipe) ? NULL
	                                                       : topipeaddr(L, 5, pipe);
	getbufarg(L, bufs);
	return uv_pipe_send(send, handle, bufs, 1, addr, uv_onsent);
}
static int pipe_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupsend, NULL);
}


/* bytes [, pipe] = pipe:receive(buffer [, i [, j]]) */
static int stoppipe (uv_pipe_t *pipe) {
	int err = uv_pipe_recv_stop(pipe);
	if (err < 0) {
		lua_State *L = (lua_State *)pipe->loop->data;
		lcu_closeobj(L, 1, (uv_handle_t *)pipe);
	}
	lcu_setpipearmed((lcu_IpcPipe *)pipe, 0);
	return err;
}
static int k_pipebuffer (lua_State *L, int status, lua_KContext ctx);
static int k_piperecv (lua_State *L, int status, lua_KContext ctx) {
	lcu_IpcPipe *pipe = openedpipe(L);
	uv_pipe_t *handle = lcu_topipehandle(pipe);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, (uv_handle_t *)handle)) {
		int err = stoppipe(handle);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	} else if (lua_isinteger(L, 6)) {
		const struct sockaddr *src = (const struct sockaddr *)lua_touserdata(L, -1);
		lua_pop(L, 1);  /* discard 'addr' lightuserdata */
		if (src) {
			struct sockaddr *dst = lcu_toaddress(L, 5);
			if (dst) {
				lcu_assert(src->sa_family == lcu_getpipeaddrfam(pipe));
				memcpy(dst, src, src->sa_family == AF_INET ? sizeof(struct sockaddr_in)
				                                           : sizeof(struct sockaddr_in6));
			}
		} else {  /* libuv indication of datagram end, just try again */
			lcuT_awaitobj(L, (uv_handle_t *)handle);
			lua_settop(L, 5);
			return lua_yieldk(L, 0, 0, k_pipebuffer);
		}
	}
	return lua_gettop(L)-5;
}
static int k_pipebuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_IpcPipe *pipe = openedpipe(L);
	uv_pipe_t *handle = lcu_topipehandle(pipe);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcuT_haltedobjop(L, (uv_handle_t *)handle)) {
		uv_buf_t *buf = (uv_buf_t *)lua_touserdata(L, -1);
		lcu_assert(buf);
		lua_pop(L, 1);  /* discard 'buf' */
		getbufarg(L, buf);
		lcuT_awaitobj(L, (uv_handle_t *)handle);
		return lua_yieldk(L, 0, 0, k_piperecv);
	} else {
		int err = stoppipe(handle);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	}
	return lua_gettop(L)-5;
}
static void uv_onpiperecv (uv_pipe_t *pipe,
                          ssize_t nread,
                          const uv_buf_t *buf,
                          const struct sockaddr *addr,
                          unsigned int flags) {
	if (buf->base != (char *)buf) {
		lua_State *thread = (lua_State *)pipe->data;
		lcu_assert(thread);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			lua_pushboolean(thread, flags&UV_UDP_PARTIAL);
			lua_pushlightuserdata(thread, (void *)addr);
		}
		else if (nread != UV_EOF) lcuL_pushresults(thread, 0, nread);
		lcuU_resumeobjop(thread, (uv_handle_t *)pipe);
	}
	if (pipe->data == NULL) stoppipe(pipe);
}
static void uv_ongetbuffer (uv_handle_t *handle,
                            size_t suggested_size,
                            uv_buf_t *buf) {
	(void)suggested_size;
	do {
		lua_State *thread = (lua_State *)handle->data;
		lcu_assert(thread);
		buf->base = (char *)buf;
		buf->len = 0;
		lua_pushlightuserdata(thread, buf);
		lcuU_resumeobjop(thread, handle);
	} while (buf->base == (char *)buf && handle->data);
}
static int pipe_receive (lua_State *L) {
	lcu_IpcPipe *pipe = ownedpipe(L, lcu_toloop(L));
	uv_pipe_t *handle = lcu_topipehandle(pipe);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (handle->data) luaL_argcheck(L, handle->data == L, 1, "already in use");
	else {
		if (!lcu_getpipearmed(pipe)) {
			int err = uv_pipe_recv_start(handle, uv_ongetbuffer, uv_onpiperecv);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_setpipearmed(pipe, 1);
		}
		lcuT_awaitobj(L, (uv_handle_t *)handle);
	}
	lua_settop(L, 5);
	return lua_yieldk(L, 0, 0, k_pipebuffer);
}


static const luaL_Reg addr[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg list[] = {
	{"__gc", resolved_close},
	{"__call", resolved_next},
	{NULL, NULL}
};

static const luaL_Reg pipe[] = {
	{"__tostring", pipe_tostring},
	{"__gc", pipe_gc},
	{"close", pipe_close},
	{"getdomain", pipe_getdomain},
	{"getaddress", pipe_getaddress},
	{"bind", pipe_bind},
	{"getoption", pipe_getoption},
	{"setoption", pipe_setoption},
	{"connect", pipe_connect},
	{"send", pipe_send},
	{"receive", pipe_receive},
	{NULL, NULL}
};

static const luaL_Reg pipe[] = {
	{"__tostring", pipe_tostring},
	{"__gc", pipe_gc},
	{"close", pipe_close},
	{"getdomain", pipe_getdomain},
	{"getaddress", pipe_getaddress},
	{"bind", pipe_bind},
	{NULL, NULL}
};

static const luaL_Reg strm[] = {
	{"getoption", pipe_getoption},
	{"setoption", pipe_setoption},
	{"connect", pipe_connect},
	{"send", pipe_send},
	{"receive", pipe_receive},
	{"shutdown", pipe_shutdown},
	{NULL, NULL}
};

static const luaL_Reg lstn[] = {
	{"listen", pipe_listen},
	{"accept", pipe_accept},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"address", system_address},
	{"findaddr", system_findaddr},
	{"nameaddr", system_nameaddr},
	{"socket", system_socket},
	{NULL, NULL}
};

LCULIB_API void lcuM_addpipef (lua_State *L) {
	lcuM_newclass(L, addr, 0, LCU_NETADDRCLS, NULL);
	lcuM_newclass(L, list, 0, LCU_NETADDRLISTCLS, NULL);
	lcuM_newclass(L, pipe, LCU_MODUPVS, LCU_UDPSOCKCLS, NULL);
	lcuM_newclass(L, pipe, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_SOCKET], NULL);
	lcuM_newclass(L, strm, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_STREAM],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_newclass(L, lstn, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_LISTEN],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
