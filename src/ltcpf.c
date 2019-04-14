#include "lmodaux.h"
#include "lhndlaux.h"


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

static void pushaddrtype (lua_State *L, int type) {
	switch (type) {
		case AF_INET: lua_pushliteral(L, "ipv4"); break;
		case AF_INET6: lua_pushliteral(L, "ipv6"); break;
		default: lua_pushliteral(L, "unsupported");
	}
}

static void setaddrport (struct sockddr *address, in_port_t port) {
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

static void setaddrbytes (struct sockaddr *address, const char *data) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			memcpy(&(addr->sin_addr.s_addr), data, LOSI_ADDRBINSZ_IPV4);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			memcpy(&addr->sin6_addr, data, LOSI_ADDRBINSZ_IPV6);
		} break;
	}
}

static const char *getaddrbytes (struct sockaddr *address, size_t *sz) {
	switch (address->sa_family) {
		case AF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in *)address;
			if (sz) *sz = LOSI_ADDRBINSZ_IPV4;
			return (const void *)&(addr->sin_addr.s_addr);
		} break;
		case AF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
			if (sz) *sz = LOSI_ADDRBINSZ_IPV6;
			return (const void *)&addr->sin6_addr;
		} break;
	}
	if (sz) *sz = 0;
	return NULL;
}

static int setaddrliteral (struct sockaddr *address, const char *data) {
	int err;
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

static const char *getaddrliteral (struct sockaddr *address, char *data) {
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
		default: return NULL;
	}
	return uv_inet_ntop(address->sa_family, bytes, data, LOSI_ADDRMAXLITERAL);
}

static const char *const AddrTypeName[] = { "ipv4", "ipv6", NULL };
static const int AddrTypeId[] = { AF_INET, AF_INET6 };
static const size_t AddrBinSz[] = { LCU_ADDRBINSZ_IPV4, LCU_ADDRBINSZ_IPV6 };

/* address [, errmsg] = system.address(type, [data [, port [, format]]]) */
static int lcuM_address (lua_State *L) {
	int type = AddrTypeId[luaL_checkoption(L, 1, NULL, AddrTypeName)];
	struct sockaddr *na = lcu_newaddress(L, type);
	int n = lua_gettop(L);
	lua_settop(L, 4);
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
				luaL_argcheck(L, sz == AddrBinSz[type], 2, "invalid binary address");
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
	char b[LCU_ADDRMAXLITERAL];
	const char *s = getaddrliteral(na, b);
	if (!s) lua_pushliteral(L, "unsupported address");
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
	if ( a1 && a2 && (getaddrtype(a1) == getaddrtype(a2)) &&
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
			char b[LCU_ADDRMAXLITERAL];
			lua_pushstring(L, getaddrliteral(na, b));
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
			size_t sz, esz = 0;
			const char *data = luamem_checkstring(L, 3, &sz);
			luaL_argcheck(L, sz == AddrBinSz[na->sa_family], 3, "wrong byte size");
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
 * Sockets
 */

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


/* succ [, errmsg] = tcp:connect(address) */
static int lcuM_tcp_connect (lua_State *L) {
	lcu_TcpSocket *tcp = livetcp(L, LCU_TCPTYPE_STREAM);
	const struct sockaddr *addr = totcpaddr(L, 2, tcp);
	int err = uv_tcp_connect(connect, &tcp->handle, addr, lcuK_onconnect);
	return losiL_doresults(L, 0, err);
}


static size_t posrelat (ptrdiff_t pos, size_t len) {
	if (pos >= 0) return (size_t)pos;
	else if (0u - (size_t)pos > len) return 0;
	else return len - ((size_t)-pos) + 1;
}

static int senddata(lua_State *L, losi_NetDriver *drv,
                                  losi_Socket *socket,
                                  const struct sockaddr *addr) {
	size_t sz, sent;
	const char *data = luamem_checkstring(L, 2, &sz);
	size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
	size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
	int err;
	if (start < 1) start = 1;
	if (end > sz) end = sz;
	sz = end - start + 1;
	data += start - 1;
	err = losiN_sendtosock(socket, data, sz, &sent, addr);
	lua_pushinteger(L, sent);
	return losiL_doresults(L, 1, err);
}

/* sent [, errmsg] = socket:send(data [, i [, j]]) */
static int stm_send (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_STRM);
	return senddata(L, socket, NULL);
}

/* sent [, errmsg] = socket:send(data [, i [, j [, address]]]) */
static int dgm_send (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_DGRM);
	return senddata(L, socket, optaddr(L, 5, socket));
}


static int recvdata(lua_State *L, losi_NetDriver *drv,
                                  losi_Socket *socket,
                                  struct sockaddr *addr) {
	size_t len, sz;
	char *buf = luamem_tomemory(L, 2, &sz);
	size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
	size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
	const char *mode = luaL_optstring(L, 5, "");
	int err;
	losi_SocketRecvFlag flags = 0;
	for (; *mode; ++mode) switch (*mode) {
		case 'p': flags |= LCU_SOCKRCV_PEEKONLY; break;
		case 'a': flags |= LCU_SOCKRCV_WAITALL; break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	if (start < 1) start = 1;
	if (end > sz) end = sz;
	sz = end - start + 1;
	buf += start - 1;
	err = losiN_recvfromsock(socket, flags, buf, sz, &len, addr);
	if (!err) lua_pushinteger(L, len);
	return losiL_doresults(L, 1, err);
}

/* data [, errmsg] = socket:receive(size [, mode]) */
static int stm_receive (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_STRM);
	return recvdata(L, socket, NULL);
}

/* data [, errmsg] = socket:receive(size [, mode [, address]]) */
static int dgm_receive (lua_State *L) {
	losi_Socket *socket = tosock(L, LCU_SOCKTYPE_DGRM);
	return recvdata(L, socket, optaddr(L, 6, socket));
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
 * Names *********************************************************************
 *****************************************************************************/


#define LCU_NETADDRFOUNDCLS LCU_PREFIX"FoundNetworkAddress"

#define tofound(L) ((losi_AddressFound *)luaL_checkudata(L, 1, \
                                         LCU_NETADDRFOUNDCLS))

/* [address, socktype, more =] next([address]) */
static int fnd_next (lua_State *L) {
	losi_AddressFound *found = tofound(L);
	struct sockaddr *addr = losi_toaddress(L, 2);
	int domain;
	SocketType type;
	int i;
	if (!losiN_getaddrtypefound(found, &domain)) return 0;
	if (addr) {
		chkaddrdom(L, 2, addr, domain);
		lua_settop(L, 2);
	} else {
		lua_settop(L, 1);
		addr = losi_newaddress(L, domain);
		losiN_initaddr(addr, domain);
	}
	losiN_getaddrfound(found, addr, &type);
	for (i = 0; SockTypeName[i] && SockTypeId[i] != type; ++i);
	lua_pushstring(L, SockTypeName[i]);
	return 2;
}

/* domain = next.domain */
static int fnd_index (lua_State *L) {
	static const char *const fields[] = {"domain",NULL};
	losi_AddressFound *found = tofound(L);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* domain */
			int type;
			if (losiN_getaddrtypefound(found, &type)) {
				switch (type) {
					case AF_INET: lua_pushliteral(L, "ipv4"); break;
					case AF_INET6: lua_pushliteral(L, "ipv6"); break;
					default: lua_pushnil(L);
				}
			}
			else lua_pushnil(L);
		} break;
	}
	return 1;
}

static int fnd_gc (lua_State *L) {
	losi_AddressFound *found = tofound(L);
	losiN_freeaddrfound(found);
	return 0;
}

/* next [, errmsg] = network.resolve (name [, service [, mode]]) */
static int net_resolve (lua_State *L) {
	const char *nodename = luaL_optstring(L, 1, NULL);
	const char *servname = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, "");
	losi_AddressFindFlag flags = 0;
	losi_AddressFound *found;
	int err;
	if (nodename) {
		if (nodename[0] == '*' && nodename[1] == '\0') {
			luaL_argcheck(L, servname, 2, "service must be provided for '*'");
			flags |= LCU_ADDRFIND_LSTN;
			nodename = NULL;
		}
	}
	else luaL_argcheck(L, servname, 1, "name or service must be provided");
	for (; *mode; ++mode) switch (*mode) {
		case '4': flags |= LCU_ADDRFIND_IPV4; break;
		case '6': flags |= LCU_ADDRFIND_IPV6; break;
		case 'm': flags |= LCU_ADDRFIND_MAP4; break;
		case 'd': flags |= LCU_ADDRFIND_DGRM; break;
		case 's': flags |= LCU_ADDRFIND_STRM; break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	found = (losi_AddressFound *)lua_newuserdata(L, sizeof(losi_AddressFound));
	err = losiN_resolveaddr(found, flags, nodename, servname);
	if (!err) luaL_setmetatable(L, LCU_NETADDRFOUNDCLS);
	return losiL_doresults(L, 1, err);
}

static char *incbuf (lua_State *L, size_t *sz) {
	lua_pop(L, 1);  /* remove old buffer */
	*sz += LCU_NETNAMEBUFSZ;
	return (char *)lua_newuserdata(L, *sz);
}

/* name [, service] = network.getname (data [, mode]) */
static int net_getname (lua_State *L) {
	char buffer[LCU_NETNAMEBUFSZ];
	size_t sz = LCU_NETNAMEBUFSZ;
	char *buf = buffer;
	int err;
	int ltype = lua_type(L, 1);
	lua_settop(L, 2);  /* discard any extra parameters */
	lua_pushnil(L);  /* simulate the initial buffer on the stack */
	if (ltype == LUA_TSTRING) {
		do {
			err = losiN_getcanonical(lua_tostring(L, 1), buf, sz);
			if (!err) {
				lua_pushstring(L, buf);
				return 1;
			} else if ((err == LCU_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
				err = 0;
			}
		} while (!err);
	} else {
		struct sockaddr *na;
		losi_AddressNameFlag flags = 0;
		const char *mode = luaL_optstring(L, 2, "");
		for (; *mode; ++mode) switch (*mode) {
			case 'l': flags |= LCU_ADDRNAME_LOCAL; break;
			case 'd': flags |= LCU_ADDRNAME_DGRM; break;
			default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
		}
		if (ltype == LUA_TNUMBER) {
			in_port_t port = int2port(L, luaL_checkinteger(L, 1), 1);
			na = losi_newaddress(L, AF_INET);
			if ( !(err = losiN_initaddr(na, AF_INET)) &&
			     !(err = losiN_setaddrport(na, port)) ) {
				do {
					err = losiN_getaddrnames(na, flags, NULL, 0, buf, sz);
					if (!err) {
						lua_pushstring(L, buf);
						return 1;
					} else if ((err == LCU_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
						err = 0;
					}
				} while (!err);
			}
		} else {
			na = toaddr(L, 1);
			do {
				size_t ssz = sz/4;
				size_t nsz = sz-ssz;
				char *sbuf = buf+nsz;
				err = losiN_getaddrnames(na, flags, buf, nsz, sbuf, ssz);
				if (!err) {
					lua_pushstring(L, buf);
					lua_pushstring(L, sbuf);
					return 2;
				} else if ((err == LCU_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
					err = 0;
				}
			} while (!err);
		}
	}
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
