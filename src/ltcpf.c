#include "looplib.h"
#include "lmodaux.h"
#include "loperaux.h"
#include "lnetwaux.h"

#include <string.h>
#include <lmemlib.h>


/*
 * Addresses 
 */

#ifndef LCU_ADDRBINSZ_IPV4
#define LCU_ADDRBINSZ_IPV4 (4*sizeof(char))
#endif

#ifndef LCU_ADDRBINSZ_IPV6
#define LCU_ADDRBINSZ_IPV6 (16*sizeof(char))
#endif

#ifndef LCU_ADDRMAXPORT
#define LCU_ADDRMAXPORT 65535
#endif

#ifndef LCU_ADDRMAXLITERAL
#define LCU_ADDRMAXLITERAL  47
#endif


static int mem2port (const char *s, const char *e, in_port_t *pn) {
	lua_Unsigned n = 0;
	do {
		char d = *s - '0';
		if (d < 0 || d > 9) return 0;  /* invalid digit */
		n = n * 10 + d;
		if (n > LCU_ADDRMAXPORT) return 0;  /* value too large */
	} while (++s < e);
	*pn = n;
	return 1;
}

static in_port_t int2port (lua_State *L, lua_Integer n, int idx) {
	luaL_argcheck(L, 0 <= n && n <= LCU_ADDRMAXPORT, idx, "invalid port");
	return (in_port_t)n;
}

static void setaddrport (struct sockaddr *address, in_port_t port) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			addr->sin_port = htons(port);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			addr->sin6_port = htons(port);
		} break;
	}
}

static in_port_t getaddrport (struct sockaddr *address) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			return ntohs(addr->sin_port);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			return ntohs(addr->sin6_port);
		} break;
	}
	return 0;
}


#define addrbinsz(T) (T == AF_INET ? LCU_ADDRBINSZ_IPV4 : LCU_ADDRBINSZ_IPV6)

static void setaddrbytes (struct sockaddr *address, const char *data) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			memcpy(&(addr->sin_addr.s_addr), data, LCU_ADDRBINSZ_IPV4);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			memcpy(&addr->sin6_addr, data, LCU_ADDRBINSZ_IPV6);
		} break;
	}
}

static const char *getaddrbytes (struct sockaddr *address, size_t *sz) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			if (sz) *sz = LCU_ADDRBINSZ_IPV4;
			return (const void *)&(addr->sin_addr.s_addr);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			if (sz) *sz = LCU_ADDRBINSZ_IPV6;
			return (const void *)&addr->sin6_addr;
		} break;
	}
	if (sz) *sz = 0;
	return NULL;
}


static int setaddrliteral (struct sockaddr *address, const char *data) {
	void *bytes;
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			bytes = (void *)&(addr->sin_addr.s_addr);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			bytes = (void *)&addr->sin6_addr;
		} break;
		default: return UV_EAFNOSUPPORT;
	}
	return uv_inet_pton(address->sa_family, data, bytes);
}

static int getaddrliteral (struct sockaddr *address, char *data) {
	const void *bytes;
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			bytes = (const void *)&(addr->sin_addr.s_addr);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			bytes = (const void *)&addr->sin6_addr;
		} break;
		default: return UV_EAI_ADDRFAMILY;
	}
	return uv_inet_ntop(address->sa_family, bytes, data, LCU_ADDRMAXLITERAL);
}


static void pushaddrtype (lua_State *L, int type) {
	switch (type) {
		case AF_INET: lua_pushliteral(L, "ipv4"); break;
		case AF_INET6: lua_pushliteral(L, "ipv6"); break;
		default: lua_pushliteral(L, "unsupported");
	}
}

static const char *const AddrTypeName[] = { "ipv4", "ipv6", NULL };
static const int AddrTypeId[] = { AF_INET, AF_INET6 };


/* address [, errmsg] = system.address(type, [data [, port [, format]]]) */
static int lcuM_address (lua_State *L) {
	int type = AddrTypeId[luaL_checkoption(L, 1, NULL, AddrTypeName)];
	int n = lua_gettop(L);
	struct sockaddr *na;
	lua_settop(L, 4);
	na = lcu_newaddress(L, type);
	lcu_assert(na);
	na->sa_family = type;
	if (n > 1) {
		in_port_t port = 0;
		size_t sz;
		const char *data = luamem_checkstring(L, 2, &sz);
		if (n == 2) {  /* URI format */
			int err;
			char literal[LCU_ADDRMAXLITERAL];
			const char *c, *e = data+sz;
			switch (type) {
				case AF_INET: {
					c = memchr(data, ':', sz - 1);  /* at least one port digit */
					luaL_argcheck(L, c, 2, "invalid URI format");
					sz = c-data;
				} break;
				case AF_INET6: {
					c = memchr(++data, ']', sz - 3);  /* intial '[' and final ':?' */
					luaL_argcheck(L, c && c[1] == ':', 2, "invalid URI format");
					sz = (c++)-data;
				} break;
				default: return lcu_error(L, UV_EAFNOSUPPORT);
			}
			luaL_argcheck(L, sz < LCU_ADDRMAXLITERAL, 2, "invalid URI format");
			luaL_argcheck(L, mem2port(c+1, e, &port), 2, "invalid port");
			memcpy(literal, data, sz);
			literal[sz] = '\0';
			err = setaddrliteral(na, literal);
			if (err) return lcu_error(L, err);
		} else {
			port = int2port(L, luaL_checkinteger(L, 3), 3);
			const char *mode = luaL_optstring(L, 4, "t");
			if (mode[0] == 'b' && mode[1] == '\0') {  /* binary format */
				luaL_argcheck(L, sz == addrbinsz(type), 2, "invalid binary address");
				setaddrbytes(na, data);
			} else if (mode[0] == 't' && mode[1] == '\0') {  /* literal format */
				int err = setaddrliteral(na, data);
				if (err) return lcu_error(L, err);
			} else {
				return luaL_argerror(L, 4, "invalid mode");
			}
		}
		setaddrport(na, port);
	}
	return 1;
}


/* uri = tostring(address) */
static int lcuM_addr_tostring (lua_State *L) {
	struct sockaddr *na = lcu_chkaddress(L, 1);
	char s[LCU_ADDRMAXLITERAL];
	int err = getaddrliteral(na, s);
	if (err) lcu_pusherror(L, err);
	else {
		in_port_t p = getaddrport(na);
		lua_pushfstring(L, na->sa_family == AF_INET6 ? "[%s]:%d" : "%s:%d", s, p);
	}
	return 1;
}


/* addr1 == addr2 */
static int lcuM_addr_eq (lua_State *L) {
	struct sockaddr *a1 = lcu_toaddress(L, 1);
	struct sockaddr *a2 = lcu_toaddress(L, 2);
	if ( a1 && a2 && (a1->sa_family == a2->sa_family) &&
	                 (getaddrport(a1) == getaddrport(a2)) ) {
		size_t sz;
		const char *b = getaddrbytes(a1, &sz);
		lua_pushboolean(L, memcmp(b, getaddrbytes(a2, NULL), sz) == 0);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/*
 * type = address.type
 * literal = address.literal
 * binary = address.binary
 * port = address.port
 */
static int lcuM_addr_index (lua_State *L) {
	static const char *const fields[] = {"port","binary","literal","type",NULL};
	struct sockaddr *na = lcu_chkaddress(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* port */
			lua_pushinteger(L, getaddrport(na));
		} break;
		case 1: {  /* binary */
			size_t sz;
			const char *s = getaddrbytes(na, &sz);
			if (s) lua_pushlstring(L, s, sz);
			else lua_pushnil(L);
		} break;
		case 2: {  /* literal */
			char s[LCU_ADDRMAXLITERAL];
			getaddrliteral(na, s);
			lua_pushstring(L, s);
		} break;
		case 3: {  /* type */
			pushaddrtype(L, na->sa_family);
		} break;
	}
	return 1;
}


/*
 * address.literal = literal
 * address.binary = binary
 * address.port = port
 */
static int lcuM_addr_newindex (lua_State *L) {
	static const char *const fields[] = { "port", "binary", "literal", NULL };
	struct sockaddr *na = lcu_chkaddress(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* port */
			setaddrport(na, int2port(L, luaL_checkinteger(L,3), 3));
		} break;
		case 1: {  /* binary */
			size_t sz;
			const char *data = luamem_checkstring(L, 3, &sz);
			luaL_argcheck(L, sz == addrbinsz(na->sa_family), 3, "wrong byte size");
			setaddrbytes(na, data);
		} break;
		case 2: {  /* literal */
			size_t sz;
			const char *data = luaL_checklstring(L, 3, &sz);
			int err = setaddrliteral(na, data);
			if (err) return lcu_error(L, err);
		} break;
	}
	return 0;
}


/*
 * TCP
 */

/* tcp [, errmsg] = system.tcp([type [, domain]]) */
static int lcuM_tcp (lua_State *L) {
	static const char *const TcpTypeName[] = { "stream", "listen", NULL };
	int class = luaL_checkoption(L, 1, "stream", TcpTypeName);
	int domain = AddrTypeId[luaL_checkoption(L, 2, "ipv4", AddrTypeName)];
	uv_loop_t *loop = lcu_toloop(L);
	lcu_TcpSocket *tcp = lcu_newtcp(L, domain, class);
	int err = uv_tcp_init_ex(loop, &tcp->handle, domain);
	if (!err) lcu_enabletcp(L, -1);
	return lcuL_doresults(L, 1, err);
}


#define totcp(L,c)	lcu_checktcp(L,1,c)

#define chktcpaddr(L,I,A,T) luaL_argcheck(L, \
                            (A)->sa_family == lcu_gettcpdom(T), I, \
                            "wrong domain")

static lcu_TcpSocket *livetcp (lua_State *L, int cls) {
	lcu_TcpSocket *tcp = totcp(L, cls);
	luaL_argcheck(L, lcu_islivetcp(tcp), 1, "closed tcp");
	return tcp;
}

static lcu_TcpSocket *ownedtcp (lua_State *L, uv_loop_t *loop, int cls) {
	lcu_TcpSocket *tcp = livetcp(L, cls);
	luaL_argcheck(L, tcp->handle.loop == loop, 1, "tcp from other system");
	return tcp;
}

static const struct sockaddr *totcpaddr (lua_State *L, int idx, \
                                         lcu_TcpSocket *tcp) {
	struct sockaddr *addr = lcu_chkaddress(L, idx);
	chktcpaddr(L, idx, addr, tcp);
	return addr;
}


/* string = tostring(tcp) */
static int lcuM_tcp_tostring (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	if (lcu_islivetcp(tcp)) lua_pushfstring(L, "tcp (%p)", tcp);
	else lua_pushliteral(L, "tcp (closed)");
	return 1;
}


/* getmetatable(tcp).__gc(tcp) */
static int lcuM_tcp_gc (lua_State *L) {
	lcu_closetcp(L, 1);
	return 0;
}


#define LCU_TCPTOKEN_WAITCONN	(void *)livetcp
#define LCU_TCPTOKEN_HAVECONN	(void *)ownedtcp

/* succ [, errmsg] = tcp:close() */
static int lcuM_tcp_close (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	if (lcu_islivetcp(tcp)) {
		int closed;
		void *data = tcp->handle.data;
		luaL_argcheck(L, data == NULL ||
		                 data == LCU_TCPTOKEN_WAITCONN ||
		                 data == LCU_TCPTOKEN_HAVECONN, 1, "still in use");
		closed = lcu_closetcp(L, 1);
		lcu_assert(closed);
		lua_pushboolean(L, closed);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/* domain = tcp:getdomain() */
static int lcuM_tcp_getdomain (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	pushaddrtype(L, lcu_gettcpdom(tcp));
	return 1;
}


/* address [, errmsg] = tcp:getaddress([site [, address]]) */
static int lcuM_tcp_getaddress (lua_State *L) {
	static const char *const sites[] = {"this", "peer", NULL};
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_SOCKET);
	int peer = luaL_checkoption(L, 2, "this", sites);
	struct sockaddr *addr = lcu_toaddress(L, 3);
	int err, addrsz;
	if (addr) {
		lua_settop(L, 3);
		chktcpaddr(L, 3, addr, tcp);
	} else {
		lua_settop(L, 2);
		addr = lcu_newaddress(L, lcu_gettcpdom(tcp));
	}
	addrsz = lua_rawlen(L, 3);
	if (peer) err = uv_tcp_getpeername(&tcp->handle, addr, &addrsz);
	else err = uv_tcp_getsockname(&tcp->handle, addr, &addrsz);
	lcu_assert(addrsz == lua_rawlen(L, 3));
	return lcuL_doresults(L, 1, err);
}


/* succ [, errmsg] = tcp:bind(address) */
static int lcuM_tcp_bind (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_SOCKET);
	const struct sockaddr *addr = totcpaddr(L, 2, tcp);
	int err = uv_tcp_bind(&tcp->handle, addr, 0);
	return lcuL_doresults(L, 0, err);
}


static const char * const TcpOptions[] = {"keepalive", "nodelay", NULL};
static const int TcpOptFlag[] = { LCU_TCPFLAG_KEEPALIVE, LCU_TCPFLAG_NODELAY };

/* succ [, errmsg] = tcp:setoption(name, value) */
static int lcuM_tcp_setoption (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	int opt = luaL_checkoption(L, 2, NULL, TcpOptions);
	int err, enabled = lua_toboolean(L, 3);
	luaL_checkany(L, 3);
	switch (opt) {
		case 0: {  /* keepalive */
			int delay = 0;
			if (enabled) delay = (int)luaL_checkinteger(L, 3);
			err = uv_tcp_keepalive(&tcp->handle, enabled, delay);
			if (!err) tcp->ka_delay = delay;
		}; break;
		case 1: {  /* nodelay */
			err = uv_tcp_nodelay(&tcp->handle, lua_toboolean(L, 3));
		}; break;
	}
	if (!err) {
		if (enabled) tcp->flags |= TcpOptFlag[opt];
		else tcp->flags &= ~(TcpOptFlag[opt]);
	}
	return lcuL_doresults(L, 0, err);
}

/* value = tcp:getoption(name) */
static int lcuM_tcp_getoption (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	int opt = luaL_checkoption(L, 2, NULL, TcpOptions);
	switch (opt) {
		case 0: {  /* keepalive */
			if (tcp->flags&LCU_TCPFLAG_KEEPALIVE) lua_pushinteger(L, tcp->ka_delay);
			else lua_pushnil(L);
		}; break;
		case 1: {  /* nodelay */
			lua_pushboolean(L, tcp->flags&LCU_TCPFLAG_NODELAY);
		}; break;
		default: break;
	}
	return 1;
}


static void lcuB_onconnected (uv_connect_t *request, int err) {
	lcu_dorequest((uv_req_t *)request, request->handle->loop, err);
}

static int lcuK_setupconnect (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, loop, NULL)) {
		lcu_PendingOp *op = lcu_getopof(L);
		uv_connect_t *request = (uv_connect_t *)lcu_torequest(op);
		lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
		const struct sockaddr *addr = totcpaddr(L, 2, tcp);
		lcu_chkinitiated(L, op, uv_tcp_connect(request, &tcp->handle, addr,
		                                       lcuB_onconnected));
		return lcu_yieldop(L, (lua_KContext)2, lcuK_chkignoreop, op);
	}
	return lua_gettop(L)-2;
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

static void lcuB_onwriten (uv_write_t *request, int err) {
	lcu_dorequest((uv_req_t *)request, request->handle->loop, err);
}

static int lcuK_setupwrite (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, loop, NULL)) {
		uv_write_t *request = (uv_write_t *)lcu_torequest(op);
		lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
		uv_stream_t *stream = (uv_stream_t *)&tcp->handle;
		size_t sz;
		const char *data = luamem_checkstring(L, 2, &sz);
		size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
		size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
		uv_buf_t bufs[1];
		if (start < 1) start = 1;
		if (end > sz) end = sz;
		bufs[0].len = end-start+1;
		bufs[0].base = (char *)(data+start-1);
		lcu_chkinitiated(L, op, uv_write(request, stream, bufs, 1, lcuB_onwriten));
		return lcu_yieldop(L, (lua_KContext)4, lcuK_chkignoreop, op);
	}
	return lua_gettop(L)-4;
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
	lua_State *co = (lua_State *)stream->data;
	lcu_assert(co);
	lua_settop(co, 0);
	if (nread >= 0) lua_pushinteger(co, nread);
	else if (nread != UV_EOF) lcuL_doresults(co, 0, nread);
	lcu_resumecoro(co, stream->loop);
}

static int stopread (lua_State *L, uv_stream_t *stream) {
	int err = uv_read_stop(stream);
	stream->data = NULL;
	lcu_freecoro(L, (void *)stream);
	return lcuL_doresults(L, lua_gettop(L), err);
}

static int lcuK_tcprecvdata (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	uv_stream_t *handle = (uv_stream_t *)&tcp->handle;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcu_doresumed(L, handle->loop, NULL)) return stopread(L, handle);
	return lua_gettop(L);
}

static int lcuK_tcpgetbuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	uv_tcp_t *handle = &tcp->handle;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, handle->loop, NULL)) {
		size_t sz;
		char *buf = luamem_checkmemory(L, 2, &sz);
		size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
		size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
		uv_buf_t *bufref = (uv_buf_t *)lua_touserdata(L, 5);
		lcu_assert(!ctx);
		lcu_assert(bufref);
		if (start < 1) start = 1;
		if (end > sz) end = sz;
		bufref->base = buf+start-1;
		bufref->len = end-start+1;
		return lcu_yieldhdl(L, 0, lcuK_tcprecvdata, (uv_handle_t *)handle);
	}
	return stopread(L, (uv_stream_t *)handle);
}

/* bytes [, errmsg] = socket:receive(buffer [, i [, j]]) */
static int lcuM_tcp_receive (lua_State *L) {
	lcu_TcpSocket *tcp = ownedtcp(L, lcu_toloop(L), LCU_TCPTYPE_STREAM);
	uv_stream_t *stream = (uv_stream_t *)&tcp->handle;
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	lua_settop(L, 4);
	if (stream->data) luaL_argcheck(L, stream->data == L, 1, "already in use");
	else lcu_chkinitiated(L, (void *)stream, uv_read_start(stream,
	                                                       luaB_ontcpwbuf,
	                                                       luaB_ontcprecv));
	return lcu_yieldhdl(L, 0, lcuK_tcpgetbuffer, (uv_handle_t*)stream);
}


static void lcuB_onshutdown (uv_shutdown_t *request, int err) {
	lcu_dorequest((uv_req_t *)request, request->handle->loop, err);
}

static int lcuK_setupshutdown (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_PendingOp *op = lcu_getopof(L);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcu_doresumed(L, loop, NULL)) {
		lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
		uv_shutdown_t *request = (uv_shutdown_t *)lcu_torequest(op);
		uv_stream_t *stream = (uv_stream_t *)&tcp->handle;
		lcu_chkinitiated(L, op, uv_shutdown(request, stream, lcuB_onshutdown));
		return lcu_yieldop(L, (lua_KContext)1, lcuK_chkignoreop, op);
	}
	return lua_gettop(L)-1;
}

/* succ [, errmsg] = socket:shutdown() */
static int lcuM_tcp_shutdown (lua_State *L) {
	lua_settop(L, 1);  /* discard extra arguments */
	lcu_resetreq(L, UV_SHUTDOWN, 0, lcuK_setupshutdown);  /* never return */
	return 0;
}


#define LCU_TCPFLAG_ACCEPTINC	(1+LCU_TCPFLAG_FLAGMASK)

static int lcuK_accepttcp (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_LISTEN);
	uv_stream_t *stream = (uv_stream_t *)&tcp->handle;
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (stream->data == (void *)L) {
		stream->data = LCU_TCPTOKEN_WAITCONN;
		lcu_freecoro(L, (void *)stream);
	}
	if (lcu_doresumed(L, stream->loop, NULL)) {
		int domain = lcu_gettcpdom(tcp);
		lcu_TcpSocket *newtcp = lcu_newtcp(L, domain, LCU_TCPTYPE_STREAM);
		uv_tcp_t *newhdl = &newtcp->handle;
		int err = uv_tcp_init(stream->loop, newhdl);
		if (!err) {
			err = uv_accept(stream, (uv_stream_t *)newhdl);
			if (!err) lcu_enabletcp(L, -1);
		}
		return lcuL_doresults(L, 1, err);
	}
	uv_unref((uv_handle_t *)stream); /* uv_listen_stop */
	return lua_gettop(L)-1;
}

static void luaB_onconection (uv_stream_t* stream, int status) {
	if (stream->data == LCU_TCPTOKEN_WAITCONN) {
		stream->data = LCU_TCPTOKEN_HAVECONN;
	} else if (stream->data == LCU_TCPTOKEN_HAVECONN) {
		lcu_TcpSocket *tcp = (lcu_TcpSocket *)stream;
		tcp->flags += LCU_TCPFLAG_ACCEPTINC;
	} else {
		lua_State *co = (lua_State *)stream->data;
		lcu_assert(co);
		lcu_assert(lua_gettop(co) == 0);
		lcu_resumecoro(co, stream->loop);
		// TODO: code below is wrong
		if (stream->data != (void *)co) uv_unref((uv_handle_t *)stream);
		// FIXED: if (stream->data == LCU_TCPTOKEN_WAITCONN) uv_unref((uv_handle_t *)stream);
	}
}

/* tcp [, errmsg] = socket:accept() */
static int lcuM_tcp_accept (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_LISTEN);
	uv_handle_t *handle = (uv_handle_t *)&tcp->handle;
	if (handle->data == LCU_TCPTOKEN_HAVECONN) {
		if (tcp->flags > LCU_TCPFLAG_FLAGMASK) tcp->flags -= LCU_TCPFLAG_ACCEPTINC;
		else handle->data = LCU_TCPTOKEN_WAITCONN;  /* no pending accepts */
		lua_pushlightuserdata(L, loop);  /* token to sign scheduled */
		return lcuK_accepttcp(L, LUA_YIELD, 0);
	} else if (handle->data != LCU_TCPTOKEN_WAITCONN) {
		lua_pushnil(L);
		lua_pushliteral(L, "unavailable");
		return 2;
	}
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	lua_settop(L, 1);
	if (!uv_has_ref(handle)) uv_ref(handle); /* uv_listen_start */
	lcu_chkinitiated(L, (void *)handle, 0);  /* savecoro */
	return lcu_yieldhdl(L, 0, lcuK_accepttcp, handle);
}

/* succ [, errmsg] = socket:listen(backlog) */
static int lcuM_tcp_listen (lua_State *L) {
	lcu_TcpSocket *tcp = ownedtcp(L, lcu_toloop(L), LCU_TCPTYPE_LISTEN);
	uv_stream_t *stream = (uv_stream_t *)&tcp->handle;
	int err, backlog = (int)luaL_checkinteger(L, 2);
	luaL_argcheck(L, 0 <= backlog && backlog < INT_MAX/LCU_TCPFLAG_ACCEPTINC, 2,
	                 "invalid backlog value");
	if (stream->data != NULL) {
		lua_pushnil(L);
		lua_pushliteral(L, "already listening");
		return 2;
	}
	lua_settop(L, 1);
	err = uv_listen(stream, backlog, luaB_onconection);
	if (!err) stream->data = LCU_TCPTOKEN_WAITCONN;
	return lcuL_doresults(L, 0, err);
}


static const luaL_Reg addr[] = {
	{"__tostring", lcuM_addr_tostring},
	{"__eq", lcuM_addr_eq},
	{"__index", lcuM_addr_index},
	{"__newindex", lcuM_addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg sock[] = {
	{"__tostring", lcuM_tcp_tostring},
	{"__gc", lcuM_tcp_gc},
	{"close", lcuM_tcp_close},
	{"getdomain", lcuM_tcp_getdomain},
	{"getaddress", lcuM_tcp_getaddress},
	{"bind", lcuM_tcp_bind},
	{NULL, NULL}
};

static const luaL_Reg strm[] = {
	{"getoption", lcuM_tcp_getoption},
	{"setoption", lcuM_tcp_setoption},
	{"connect", lcuM_tcp_connect},
	{"send", lcuM_tcp_send},
	{"receive", lcuM_tcp_receive},
	{"shutdown", lcuM_tcp_shutdown},
	{NULL, NULL}
};

static const luaL_Reg lstn[] = {
	{"listen", lcuM_tcp_listen},
	{"accept", lcuM_tcp_accept},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"address", lcuM_address},
	{"tcp", lcuM_tcp},
	{NULL, NULL}
};

LCULIB_API void lcuM_addtcpf (lua_State *L) {
	lcuM_newclass(L, addr, 0, LCU_NETADDRCLS, NULL);
	lcuM_newclass(L, sock, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_SOCKET], NULL);
	lcuM_newclass(L, strm, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_STREAM],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_newclass(L, lstn, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_LISTEN],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
