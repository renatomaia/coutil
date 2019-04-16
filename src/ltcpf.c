#include "lmodaux.h"
#include "lhndlaux.h"


static const char *const TcpTypeName[] = { "stream", "listen", NULL };

/* tcp [, errmsg] = system.tcp([type [, domain]]) */
static int lcuM_tcp (lua_State *L) {
	int class = luaL_checkoption(L, 1, "stream", TcpTypeName);
	int domain = AddrTypeId[luaL_checkoption(L, 2, "ipv4", AddrTypeName)];
	uv_loop_t *loop = lcu_toloop(L);
	lcu_TcpSocket *tcp = lcu_createtcp(L, domain, class);
	int err = uv_tcp_init_ex(loop, &tcp->handle, domain);
	if (!err) lcu_enabletcp(L, -1);
	return lcuL_doresults(L, 1, err);
}


#define totcp(L,c)	((lcu_TcpSocket *)luaL_checkinstance(L, 1, \
                                                        lcu_TcpClasses[c]))

static lcu_TcpSocket *livetcp (lua_State *L, int cls) {
	lcu_TcpSocket *tcp = totcp(L, cls);
	luaL_argcheck(L, lcu_islivetcp(tcp), 1, "closed tcp");
	return tcp;
}

static lcu_TcpSocket *ownedtcp (lua_State *L, int cls) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_TcpSocket *tcp = livetcp(L, cls);
	luaL_argcheck(L, tcp->handle.loop == loop, 1, "tcp from other system");
	return tcp;
}

#define chkaddrdom(L,I,A,D) luaL_argcheck(L, (A)->sa_family == D, I, \
                                             "wrong domain")

static const struct sockaddr *totcpaddr (lua_State *L, int idx, \
                                         lcu_TcpSocket *tcp) {
	struct sockaddr *addr = toaddr(L, idx);
	chktcpaddr(L, idx, addr, tcp);
	return addr;
}

static struct sockaddr *optaddr (lua_State *L, int idx, lcu_TcpSocket *tcp) {
	struct sockaddr *addr = lcu_toaddress(L, idx);
	if (addr) chkaddrdom(L, idx, addr, lcu_gettcpdom(tcp));
	return addr;
}


/* string = tostring(tcp) */
static int lcuM_tcp_tostring (lua_State *L) {
	lcu_TcpSocket *tcp = lcu_chktcp(L, 1, LCU_TCPTYPE_ANY);
	if (lcu_islivetcp(tcp)) lua_pushfstring(L, "tcp (%p)", tcp);
	else lua_pushliteral(L, "tcp (closed)");
	return 1;
}


/* getmetatable(tcp).__gc(tcp) */
static int lcuM_tcp_gc (lua_State *L) {
	lcu_closetcp(L, 1);
	return 0;
}


/* succ [, errmsg] = tcp:close() */
static int lcuM_tcp_close (lua_State *L) {
	lcu_chktcp(L, 1, LCU_TCPTYPE_ANY);
	lua_pushboolean(L, lcu_closetcp(L, 1));
	return 1;
}


/* domain = tcp:getdomain() */
static int lcuM_tcp_getdomain (lua_State *L) {
	lcu_TcpSocket *tcp = lcu_chktcp(L, 1, LCU_TCPTYPE_ANY);
	pushaddrtype(L, lcu_gettcpdom(tcp));
	return 1;
}


/* succ [, errmsg] = tcp:bind(address) */
static int lcuM_tcp_bind (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_ANY);
	const struct sockaddr *addr = totcpaddr(L, 2, tcp);
	int err = uv_tcp_bind(&tcp->handle, addr, 0);
	return lcuL_doresults(L, 0, err);
}


/* address [, errmsg] = tcp:getaddress([site [, address]]) */
static int lcuM_tcp_getaddress (lua_State *L) {
	static const char *const sites[] = {"this", "peer", NULL};
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_ANY);
	int peer = luaL_checkoption(L, 2, "this", sites);
	int domain = lcu_gettcpdom(tcp);
	struct sockaddr *addr = lcu_toaddress(L, 3);
	if (addr) {
		lua_settop(L, 3);
		chkaddrdom(L, 3, addr, domain);
	} else {
		lua_settop(L, 2);
		addr = lcu_newaddress(L, domain);
	}
	if (peer) err = uv_tcp_getpeername(&tcp->handle, addr, lua_rawlen(L, 3));
	else err = uv_tcp_getsockname(&tcp->handle, addr, lua_rawlen(L, 3));
	return lcuL_doresults(L, 1, err);
}


static const char * const TcpOptions[] = {"keepalive", "nodelay", NULL};
static const int TcpOptFlag[] = { LCU_TCPKEEPALIVE_FLAG, LCU_TCPNODELAY_FLAG };

/* succ [, errmsg] = tcp:setoption(name, value) */
static int lcuM_tcp_setoption (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_ANY);
	int opt = luaL_checkoption(L, 2, TcpOptions);
	int err;
	luaL_checkany(L, 3);
	switch (opt) {
		case 0: {  /* keepalive */
			int delay = 0;
			int enabled = lua_toboolean(L, 3)
			if (enable) delay = (int)luaL_checkinteger(L, 3);
			err = uv_tcp_keepalive(&tcp->handle, enable, delay);
			if (!err) tcp->ka_delay = delay;
		}; break;
		case 1: {  /* nodelay */
			err = uv_tcp_nodelay(&tcp->handle, lua_toboolean(L, 3));
		}; break;
	}
	if (err) lcuL_doresults(L, 0, err);
	tcp->flags |= TcpOptFlag[opt];
	return 0;
}

/* value = tcp:getoption(name) */
static int lcuM_tcp_getoption (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_ANY);
	int opt = luaL_checkoption(L, 2, TcpOptions);
	switch (opt) {
		case 0: {  /* keepalive */
			if (tcp->flags&LCU_TCPKEEPALIVE_FLAG) lua_pushinteger(L, tcp->ka_delay);
			else lua_pushnil(L);
		}; break;
		case 1: {  /* nodelay */
			lua_pushboolean(L, tcp->flags&LCU_TCPNODELAY_FLAG);
		}; break;
		default: break;
	}
	return 1;
}


static int lcuB_onconnected (uv_connect_t* request, int err) {
	lua_State *co = (lua_State *)request->data;
	lcu_assert(co != NULL);
	lcu_PendingOp *op = (lcu_PendingOp *)request;
	lcu_freereq(op);
	lua_settop(co, 0);
	lcuL_doresults(co, 0, err);
	lcu_resumeop(op, co);
}

static int lcuK_setupconnect (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, loop, op)) {
		uv_connect_t *request = (uv_connect_t *)lcu_torequest(op);
		lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
		const struct sockaddr *addr = totcpaddr(L, 2, tcp);
		lcu_chkinitop(L, op, loop, uv_tcp_connect(request, &tcp->handle, addr,
		                                          lcuB_onconnected));
		return lcu_yieldop(L, 0, lcuK_chkignoreop, op);
	}
	return lua_gettop(L);
}

/* succ [, errmsg] = tcp:connect(address) */
static int lcuM_tcp_connect (lua_State *L) {
	lua_settop(L, 2);  /* discard extra arguments */
	lcu_resetreq(L, UV_CONNECT, 0, lcuK_setupconnect);  /* never return */
	return 0;
}


static size_t posrelat (ptrdiff_t pos, size_t len) {
	if (pos >= 0) return (size_t)pos;
	else if (0u - (size_t)pos > len) return 0;
	else return len - ((size_t)-pos) + 1;
}

static int lcuB_onwriten (uv_write_t* request, int err) {
	lua_State *co = (lua_State *)request->data;
	lcu_assert(co != NULL);
	lcu_PendingOp *op = (lcu_PendingOp *)request;
	lcu_freereq(op);
	lua_settop(co, 0);
	lcuL_doresults(co, 0, err);
	lcu_resumeop(op, co);
}

static int lcuK_setupwrite (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, loop, op)) {
		uv_write_t *request = (uv_write_t *)lcu_torequest(op);
		lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
		size_t sz, sent;
		const char *data = luamem_checkstring(L, 2, &sz);
		size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
		size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
		uv_buf_t bufs[1];
		int err;
		if (start < 1) start = 1;
		if (end > sz) end = sz;
		bufs[0].len = end-start+1;
		bufs[0].base = data+start-1;
		err = uv_write(request, &tcp->handle, bufs, 1, lcuB_onwriten);
		return lcu_yieldop(L, 0, lcuK_chkignoreop, op);
	}
	return lua_gettop(L);
}

/* sent [, errmsg] = tcp:send(data [, i [, j]]) */
static int lcuM_tcp_send (lua_State *L) {
	lua_settop(L, 4);  /* discard extra arguments */
	lcu_resetreq(L, UV_WRITE, 0, lcuK_setupwrite);  /* never return */
	return 0;
}


static void luaB_ontcpwbuf (uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t *bufref) {
	lua_State *co = (lua_State *)handle->data;
	lcu_assert(co);
	lua_pushlightuserdata(co, bufref);
	lcu_resumecoro(co, handle->loop);
}

static void luaB_ontcprecv (uv_stream_t* stream,
                            ssize_t nread,
                            const uv_buf_t* buf) {
	lua_State *co = (lua_State *)handle->data;
	lcu_assert(co);
	lua_settop(L, 0);
	if (nread >= 0) lua_pushinteger(co, nread);
	else if (nread != UV_EOF) lcuL_doresults(co, 0, nread);
	lcu_resumecoro(co, handle->loop);
}

static int lcuK_tcprecvdata (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcu_doresumed(L, lcu_toloop(L)))
		uv_read_stop((uv_stream_t*)&tcp->handle);
	return lua_gettop(L);
}

static int lcuK_tcpgetbuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, lcu_toloop(L))) {
		size_t len, sz;
		char *buf = luamem_checkmemory(L, 2, &sz);
		size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
		size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
		uv_buf_t *bufref = (uv_buf_t *)lua_touserdata(L, 5);
		lcu_assert(!ctx);
		lcu_assert(bufref);
		if (start < 1) start = 1;
		if (end > sz) end = sz;
		bufref->base = buf+start-1
		bufref->len = end-start+1;
		return lcu_yieldhdl(L, 0, lcuK_tcprecvdata, &tcp->handle);
	}
	uv_read_stop((uv_stream_t*)&tcp->handle);
	return lua_gettop(L);
}

/* bytes [, errmsg] = socket:receive(buffer [, i [, j]]) */
static int lcuM_tcp_receive (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	lcu_assert(!lcu_ispendingop(lcu_getopof(L)));
	luaL_argcheck(L, !uv_is_active((uv_handle_t*)&tcp->handle), 1, "in use");
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	lua_settop(L, 4);
	lcu_chkinithdl(L, &tcp->handle, uv_read_start((uv_stream_t*)&tcp->handle,
	                                              luaB_ontcpwbuf,
	                                              luaB_ontcprecv));
	return lcu_yieldop(L, 0, lcuK_tcpgetbuffer, (uv_handle_t*)&tcp->handle);
}


/* succ [, errmsg] = socket:shutdown([mode]) */
static int stm_shutdown (lua_State *L) {
	static const char *const waynames[] = {"send", "receive", "both", NULL};
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_STRM);
	int way = luaL_checkoption(L, 2, "both", waynames);
	int err = losiN_shutdownsock(socket, way);
	return losiL_doresults(L, 0, err);
}


/* socket [, errmsg] = socket:accept([address]) */
static int lst_accept (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_LSTN);
	struct sockaddr *addr = optaddr(L, 2, socket);
	losi_Socket *conn = losi_newsocket(L, LCU_SOCKTYPE_STRM);
	int err = losiN_acceptsock(socket, conn, addr);
	if (!err) losi_enablesocket(L, -1);
	return losiL_doresults(L, 1, err);
}


/* succ [, errmsg] = socket:listen([backlog]) */
static int lst_listen (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_LSTN);
	int backlog = luaL_optinteger(L, 2, 32);
	int err = losiN_listensock(socket, backlog);
	return losiL_doresults(L, 0, err);
}


/*****************************************************************************
 * Library *******************************************************************
 *****************************************************************************/


static int net_type (lua_State *L) {
	if (lua_isuserdata(L, 1) && lua_getmetatable(L, 1)) {
		luaL_getmetatable(L, LCU_NETADDRCLS);
		if (lua_rawequal(L, 2, 3)) {
			lua_pushliteral(L, "address");
			return 1;
		}
		lua_pop(L, 1);  /* remove address metatable */
		if (luaL_issubclass(L, losi_SocketClasses[LCU_SOCKTYPE_SOCK])) {
			lua_pushliteral(L, "socket");
			return 1;
		}
	}
	return 0;
}


static const luaL_Reg addr[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg fnd[] = {
	{"__gc", fnd_gc},
	{"__call", fnd_next},
	{"__index", fnd_index},
	{NULL, NULL}
};

static const luaL_Reg sck[] = {
	{"__tostring", sck_tostring},
	{"__gc", sck_gc},
	{"close", sck_close},
	{"getdomain", sck_getdomain},
	{"getaddress", sck_getaddress},
	{"bind", sck_bind},
	{NULL, NULL}
};

static const luaL_Reg lst[] = {
	{"getoption", lst_getoption},
	{"setoption", lst_setoption},
	{"accept", lst_accept},
	{"listen", lst_listen},
	{NULL, NULL}
};

static const luaL_Reg trs[] = {
	{"connect", trs_connect},
	{NULL, NULL}
};

static const luaL_Reg stm[] = {
	{"getoption", stm_getoption},
	{"setoption", stm_setoption},
	{"send", stm_send},
	{"receive", stm_receive},
	{"shutdown", stm_shutdown},
	{NULL, NULL}
};

static const luaL_Reg dgm[] = {
	{"getoption", dgm_getoption},
	{"setoption", dgm_setoption},
	{"send", dgm_send},
	{"receive", dgm_receive},
	{NULL, NULL}
};

static const luaL_Reg lib[] = {
	{"address", net_address},
	{"getname", net_getname},
	{"resolve", net_resolve},
	{"socket", net_socket},
	{"type", net_type},
	{NULL, NULL}
};

#ifndef LCU_DISABLE_NETDRV
static int lfreedrv (lua_State *L) {
	losi_NetDriver *drv = (losi_NetDriver *)lua_touserdata(L, 1);
	losiN_freedrv(drv);
	return 0;
}
#endif

LUAMOD_API int luaopen_network (lua_State *L) {
#ifndef LCU_DISABLE_NETDRV
	/* create sentinel */
	losi_NetDriver *drv;
	int err;
	lua_settop(L, 0);  /* dicard any arguments */
	drv = (losi_NetDriver *)luaL_newsentinel(L, sizeof(losi_NetDriver),
	                                            lfreedrv);
	/* initialize library */
	err = losiN_initdrv(drv);
	if (err) {
		luaL_cancelsentinel(L);
		losiL_pusherrmsg(L, err);
		return lua_error(L);
	}
#define pushsentinel(L)	lua_pushvalue(L, 1)
#else
#define pushsentinel(L)	((void)L)
#endif
	/* create address class */
	pushsentinel(L);
	luaL_newclass(L, LCU_NETADDRCLS, NULL, addr, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create found address class */
	pushsentinel(L);
	luaL_newclass(L, LCU_NETADDRFOUNDCLS, NULL, fnd, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create abstract base socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LCU_SOCKTYPE_SOCK], NULL, sck, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create listening socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LCU_SOCKTYPE_LSTN],
	                 losi_SocketClasses[LCU_SOCKTYPE_SOCK], lst, DRVUPV);
	lua_pushliteral(L, "listen");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create data transfer socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LCU_SOCKTYPE_TRSP],
	                 losi_SocketClasses[LCU_SOCKTYPE_SOCK], trs, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create stream socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LCU_SOCKTYPE_STRM],
	                 losi_SocketClasses[LCU_SOCKTYPE_TRSP], stm, DRVUPV);
	lua_pushliteral(L, "stream");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create datagram socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LCU_SOCKTYPE_DGRM],
	                 losi_SocketClasses[LCU_SOCKTYPE_TRSP], dgm, DRVUPV);
	lua_pushliteral(L, "datagram");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create library table */
	luaL_newlibtable(L, lib);
	pushsentinel(L);
	luaL_setfuncs(L, lib, DRVUPV);

#ifdef LCU_ENABLE_SOCKETEVENTS
	losi_defgetevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_LSTN],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_LSTN],
	                      losiN_freesockevtsrc);
	losi_defgetevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_STRM],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_STRM],
	                      losiN_freesockevtsrc);
	losi_defgetevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_DGRM],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LCU_SOCKTYPE_DGRM],
	                      losiN_freesockevtsrc);
#endif
	return 1;
}
