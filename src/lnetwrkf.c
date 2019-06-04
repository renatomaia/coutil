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
#define LCU_ADDRBINSZ_IPV4 (sizeof(struct in_addr))
#endif

#ifndef LCU_ADDRBINSZ_IPV6
#define LCU_ADDRBINSZ_IPV6 (sizeof(struct in6_addr))
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
static int system_address (lua_State *L) {
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
static int addr_tostring (lua_State *L) {
	struct sockaddr *na = lcu_checkaddress(L, 1);
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
static int addr_eq (lua_State *L) {
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
static int addr_index (lua_State *L) {
	static const char *const fields[] = {"port","binary","literal","type",NULL};
	struct sockaddr *na = lcu_checkaddress(L, 1);
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
static int addr_newindex (lua_State *L) {
	static const char *const fields[] = { "port", "binary", "literal", NULL };
	struct sockaddr *na = lcu_checkaddress(L, 1);
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
 * Network
 */

/* socket [, errmsg] = system.socket(type [, domain]) */
static int system_socket (lua_State *L) {
	static const char *const TcpTypeName[] = { "stream", "listen", "datagram", NULL };
	int class = luaL_checkoption(L, 1, NULL, TcpTypeName);
	int domain = AddrTypeId[luaL_checkoption(L, 2, "ipv4", AddrTypeName)];
	uv_loop_t *loop = lcu_toloop(L);
	int err;
	switch (class) {
		case 0:
		case 1: {
			lcu_TcpSocket *tcp = lcu_newtcp(L, domain, class ? LCU_TCPTYPE_LISTEN
			                                                 : LCU_TCPTYPE_STREAM);
			err = uv_tcp_init_ex(loop, lcu_totcphandle(tcp), domain);
			if (!err) lcu_enabletcp(L, -1);
		} break;
		case 2: {
			lcu_UdpSocket *udp = lcu_newudp(L, domain);
			err = uv_udp_init_ex(loop, lcu_toudphandle(udp), domain);
			if (!err) lcu_enableudp(L, -1);
		} break;
		default: err = UV_EAI_SOCKTYPE;
	}
	return lcuL_pushresults(L, 1, err);
}

#define chkaddrdom(L,I,A,D) luaL_argcheck(L, (A)->sa_family == D, I, "wrong domain")

static int getaddrarg (lua_State *L, int domain, struct sockaddr **addr, int *sz) {
	static const char *const sites[] = {"this", "peer", NULL};
	int peer = luaL_checkoption(L, 2, "this", sites);
	*addr = lcu_toaddress(L, 3);
	if (*addr) {
		lua_settop(L, 3);
		chkaddrdom(L, 3, *addr, domain);
	} else {
		lua_settop(L, 2);
		*addr = lcu_newaddress(L, domain);
	}
	*sz = (int)lua_rawlen(L, 3);
	return peer;
}

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



/*
 * UDP
 */

#define toudp(L)	lcu_checkudp(L,1)

static lcu_UdpSocket *openedudp (lua_State *L) {
	lcu_UdpSocket *udp = toudp(L);
	luaL_argcheck(L, !lcu_isudpclosed(udp), 1, "closed udp");
	return udp;
}

static lcu_UdpSocket *ownedudp (lua_State *L, uv_loop_t *loop) {
	lcu_UdpSocket *udp = openedudp(L);
	luaL_argcheck(L, lcu_toudphandle(udp)->loop == loop, 1, "foreign object");
	return udp;
}

static const struct sockaddr *toudpaddr (lua_State *L, int idx, \
                                         lcu_UdpSocket *udp) {
	struct sockaddr *addr = lcu_checkaddress(L, idx);
	chkaddrdom(L, idx, addr, lcu_getudpaddrfam(udp));
	return addr;
}


/* string = tostring(udp) */
static int udp_tostring (lua_State *L) {
	lcu_UdpSocket *udp = toudp(L);
	if (!lcu_isudpclosed(udp)) lua_pushfstring(L, "udp (%p)", udp);
	else lua_pushliteral(L, "udp (closed)");
	return 1;
}


/* getmetatable(udp).__gc(udp) */
static int udp_gc (lua_State *L) {
	lcu_closeudp(L, 1);
	return 0;
}


/* succ [, errmsg] = udp:close() */
static int udp_close (lua_State *L) {
	lcu_UdpSocket *udp = toudp(L);
	if (!lcu_isudpclosed(udp)) {
		int closed = lcu_closeudp(L, 1);
		lcu_assert(closed);
		lua_pushboolean(L, closed);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/* domain = udp:getdomain() */
static int udp_getdomain (lua_State *L) {
	lcu_UdpSocket *udp = toudp(L);
	pushaddrtype(L, lcu_getudpaddrfam(udp));
	return 1;
}


/* address [, errmsg] = udp:getaddress([site [, address]]) */
static int udp_getaddress (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphandle(udp);
	struct sockaddr *addr;
	int addrsz;
	int peer = getaddrarg(L, lcu_getudpaddrfam(udp), &addr, &addrsz);
	int err;
	if (peer)
#ifdef uv_udp_getpeername
		err = uv_udp_getpeername(handle, addr, &addrsz);
#else
		err = UV_ENOTSUP;
#endif
	else err = uv_udp_getsockname(handle, addr, &addrsz);
	lcu_assert(addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = udp:bind(address) */
static int udp_bind (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	const struct sockaddr *addr = toudpaddr(L, 2, udp);
	int err = uv_udp_bind(lcu_toudphandle(udp), addr, 0);
	return lcuL_pushresults(L, 0, err);
}


static const char * const UdpOptions[] = {
	"broadcast",
	"mcastloop",
	"mcastttl",
	"mcastiface",
	NULL
};

/* succ [, errmsg] = udp:setoption(name, value) */
static int udp_setoption (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphandle(udp);
	int opt = luaL_checkoption(L, 2, NULL, UdpOptions);
	int err;
	switch (opt) {
		case 0: {  /* broadcast */
			int enabled = lua_toboolean(L, 3);
			err = uv_udp_set_broadcast(handle, enabled);
			if (err >= 0) lcu_setudpbroadcast(udp, enabled);
		}; break;
		case 1: {  /* mcastloop */
			int enabled = lua_toboolean(L, 3);
			err = uv_udp_set_multicast_loop(handle, enabled);
			if (err >= 0) lcu_setudpmcastloop(udp, enabled);
		}; break;
		case 2: {  /* mcastttl */
			lua_Integer value = luaL_checkinteger(L, 3);
			luaL_argcheck(L, 0 < value && value < 256, 3, "must be from 1 upto 255");
			err = uv_udp_set_multicast_ttl(handle, (int)value);
			if (err >= 0) lcu_setudpmcastttl(udp, (int)value);
		}; break;
		case 3: {  /* mcastiface */
			int domain = lcu_getudpaddrfam(udp);
			size_t len;
			const char *addr = luaL_checklstring(L, 3, &len);
			char buffer[LCU_ADDRMAXLITERAL];
			int err = 0;
			luaL_argcheck(L, len == addrbinsz(domain), 3, "invalid binary address");
			err = uv_inet_ntop(domain, (void *)addr, buffer, LCU_ADDRMAXLITERAL);
			if (err >= 0) {
				err = uv_udp_set_multicast_interface(handle, buffer);
				if (err >= 0) lcu_setudpmcastiface(udp, addr, len);
			}
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}

/* value = udp:getoption(name) */
static int udp_getoption (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	int opt = luaL_checkoption(L, 2, NULL, UdpOptions);
	switch (opt) {
		case 0: {  /* broadcast */
			lua_pushboolean(L, lcu_getudpbroadcast(udp));
		}; break;
		case 1: {  /* mcastloop */
			lua_pushboolean(L, lcu_getudpmcastloop(udp));
		}; break;
		case 2: {  /* mcastttl */
			lua_pushinteger(L, lcu_getudpmcastttl(udp));
		}; break;
		case 3: {  /* mcastiface */
			size_t sz;
			const char *value = lcu_getudpmcastiface(udp, &sz);
			lua_pushlstring(L, value, sz);
		}; break;
		default: return 0;
	}
	return 1;
}


/* succ [, errmsg] = udp:connect([address]) */
static int udp_connect (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	const struct sockaddr *addr = lua_isnil(L, 2) ? NULL : toudpaddr(L, 2, udp);
	int err = uv_udp_connect(lcu_toudphandle(udp), addr);
	if (err >= 0) lcu_setudpconnected(udp, addr != NULL);
	return lcuL_pushresults(L, 0, err);
}



/* sent [, errmsg] = udp:send(data [, i [, j [, address]]]) */
static void uv_onsent (uv_udp_send_t *request, int err) {
	lcuU_resumereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupsend (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_UdpSocket *udp = ownedudp(L, loop);
	uv_udp_send_t *send = (uv_udp_send_t *)request;
	uv_udp_t *handle = (uv_udp_t *)lcu_toudphandle(udp);
	uv_buf_t bufs[1];
	const struct sockaddr *addr = lcu_getudpconnected(udp) ? NULL
	                                                       : toudpaddr(L, 5, udp);
	getbufarg(L, bufs);
	return uv_udp_send(send, handle, bufs, 1, addr, uv_onsent);
}
static int udp_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupsend, NULL);
}


/* bytes [, errmsg] = udp:receive(buffer [, i [, j [, address]]]) */
static int stopudp (uv_udp_t *udp) {
	int err = uv_udp_recv_stop(udp);
	if (err < 0) {
		lua_State *L = (lua_State *)udp->loop->data;
		lcu_closeobj(L, 1, (uv_handle_t *)udp);
	}
	lcu_setudparmed((lcu_UdpSocket *)udp, 0);
	return err;
}
static int k_udpbuffer (lua_State *L, int status, lua_KContext ctx);
static int k_udprecv (lua_State *L, int status, lua_KContext ctx) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphandle(udp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, (uv_handle_t *)handle)) {
		int err = stopudp(handle);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	} else if (lua_isinteger(L, 6)) {
		const struct sockaddr *src = (const struct sockaddr *)lua_touserdata(L, -1);
		lua_pop(L, 1);  /* discard 'addr' lightuserdata */
		if (src) {
			struct sockaddr *dst = lcu_toaddress(L, 5);
			if (dst) {
				lcu_assert(src->sa_family == lcu_getudpaddrfam(udp));
				memcpy(dst, src, src->sa_family == AF_INET ? sizeof(struct sockaddr_in)
				                                           : sizeof(struct sockaddr_in6));
			}
		} else {  /* libuv indication of datagram end, just try again */
			lcuT_awaitobj(L, (uv_handle_t *)handle);
			lua_settop(L, 5);
			return lua_yieldk(L, 0, 0, k_udpbuffer);
		}
	}
	return lua_gettop(L)-5;
}
static int k_udpbuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphandle(udp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcuT_haltedobjop(L, (uv_handle_t *)handle)) {
		uv_buf_t *buf = (uv_buf_t *)lua_touserdata(L, -1);
		lcu_assert(buf);
		lua_pop(L, 1);  /* discard 'buf' */
		getbufarg(L, buf);
		lcuT_awaitobj(L, (uv_handle_t *)handle);
		return lua_yieldk(L, 0, 0, k_udprecv);
	} else {
		int err = stopudp(handle);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	}
	return lua_gettop(L)-5;
}
static void uv_onudprecv (uv_udp_t *udp,
                          ssize_t nread,
                          const uv_buf_t *buf,
                          const struct sockaddr *addr,
                          unsigned int flags) {
	if (buf->base != (char *)buf) {
		lua_State *thread = (lua_State *)udp->data;
		lcu_assert(thread);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			lua_pushlightuserdata(thread, (void *)addr);
		}
		else if (nread != UV_EOF) lcuL_pushresults(thread, 0, nread);
		lcuU_resumeobjop(thread, (uv_handle_t *)udp);
	}
	if (udp->data == NULL) stopudp(udp);
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
static int udp_receive (lua_State *L) {
	lcu_UdpSocket *udp = ownedudp(L, lcu_toloop(L));
	uv_udp_t *handle = lcu_toudphandle(udp);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (handle->data) luaL_argcheck(L, handle->data == L, 1, "already in use");
	else {
		if (!lcu_getudparmed(udp)) {
			int err = uv_udp_recv_start(handle, uv_ongetbuffer, uv_onudprecv);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_setudparmed(udp, 1);
		}
		lcuT_awaitobj(L, (uv_handle_t *)handle);
	}
	lua_settop(L, 5);
	return lua_yieldk(L, 0, 0, k_udpbuffer);
}


/*
 * TCP
 */

#define totcp(L,c)	lcu_checktcp(L,1,c)

static lcu_TcpSocket *openedtcp (lua_State *L, int cls) {
	lcu_TcpSocket *tcp = totcp(L, cls);
	luaL_argcheck(L, !lcu_istcpclosed(tcp), 1, "closed tcp");
	return tcp;
}

static lcu_TcpSocket *ownedtcp (lua_State *L, uv_loop_t *loop, int cls) {
	lcu_TcpSocket *tcp = openedtcp(L, cls);
	luaL_argcheck(L, lcu_totcphandle(tcp)->loop == loop, 1, "foreign object");
	return tcp;
}

static const struct sockaddr *totcpaddr (lua_State *L, int idx, \
                                         lcu_TcpSocket *tcp) {
	struct sockaddr *addr = lcu_checkaddress(L, idx);
	chkaddrdom(L, idx, addr, lcu_gettcpaddrfam(tcp));
	return addr;
}


/* string = tostring(tcp) */
static int tcp_tostring (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	if (!lcu_istcpclosed(tcp)) lua_pushfstring(L, "tcp (%p)", tcp);
	else lua_pushliteral(L, "tcp (closed)");
	return 1;
}


/* getmetatable(tcp).__gc(tcp) */
static int tcp_gc (lua_State *L) {
	lcu_closetcp(L, 1);
	return 0;
}


/* succ [, errmsg] = tcp:close() */
static int tcp_close (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	if (!lcu_istcpclosed(tcp)) {
		int closed = lcu_closetcp(L, 1);
		lcu_assert(closed);
		lua_pushboolean(L, closed);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/* domain = tcp:getdomain() */
static int tcp_getdomain (lua_State *L) {
	lcu_TcpSocket *tcp = totcp(L, LCU_TCPTYPE_SOCKET);
	pushaddrtype(L, lcu_gettcpaddrfam(tcp));
	return 1;
}


/* address [, errmsg] = tcp:getaddress([site [, address]]) */
static int tcp_getaddress (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_SOCKET);
	uv_tcp_t *handle = lcu_totcphandle(tcp);
	struct sockaddr *addr;
	int addrsz;
	int peer = getaddrarg(L, lcu_gettcpaddrfam(tcp), &addr, &addrsz);
	int err;
	if (peer) err = uv_tcp_getpeername(handle, addr, &addrsz);
	else err = uv_tcp_getsockname(handle, addr, &addrsz);
	lcu_assert(addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = tcp:bind(address) */
static int tcp_bind (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_SOCKET);
	const struct sockaddr *addr = totcpaddr(L, 2, tcp);
	int err = uv_tcp_bind(lcu_totcphandle(tcp), addr, 0);
	return lcuL_pushresults(L, 0, err);
}


static const char * const TcpOptions[] = {"keepalive", "nodelay", NULL};

/* succ [, errmsg] = tcp:setoption(name, value) */
static int tcp_setoption (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_STREAM);
	int opt = luaL_checkoption(L, 2, NULL, TcpOptions);
	int err, enabled = lua_toboolean(L, 3);
	luaL_checkany(L, 3);
	switch (opt) {
		case 0: {  /* keepalive */
			int delay = -1;
			if (enabled) {
				delay = (int)luaL_checkinteger(L, 3);
				luaL_argcheck(L, delay >= 0, 3, "negative delay");
			}
			err = uv_tcp_keepalive(lcu_totcphandle(tcp), enabled, delay);
			if (err >= 0) lcu_settcpkeepalive(tcp, delay);
		}; break;
		case 1: {  /* nodelay */
			err = uv_tcp_nodelay(lcu_totcphandle(tcp), enabled);
			if (err >= 0) lcu_settcpnodelay(tcp, enabled);
		}; break;
	}
	return lcuL_pushresults(L, 0, err);
}

/* value = tcp:getoption(name) */
static int tcp_getoption (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_STREAM);
	int opt = luaL_checkoption(L, 2, NULL, TcpOptions);
	switch (opt) {
		case 0: {  /* keepalive */
			int delay = lcu_gettcpkeepalive(tcp);
			if (delay < 0) lua_pushnil(L);
			else lua_pushinteger(L, delay);
		}; break;
		case 1: {  /* nodelay */
			lua_pushboolean(L, lcu_gettcpnodelay(tcp));
		}; break;
		default: return 0;
	}
	return 1;
}


/* succ [, errmsg] = tcp:connect(address) */
static void uv_onconnected (uv_connect_t *request, int err) {
	lcuU_resumereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupconnect (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
	const struct sockaddr *addr = totcpaddr(L, 2, tcp);
	uv_connect_t *connect = (uv_connect_t *)request;
	return uv_tcp_connect(connect, lcu_totcphandle(tcp), addr, uv_onconnected);
}
static int tcp_connect (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupconnect, NULL);
}


/* succ [, errmsg] = socket:shutdown() */
static void uv_onshutdown (uv_shutdown_t *request, int err) {
	lcuU_resumereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupshutdown (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
	uv_shutdown_t *shutdown = (uv_shutdown_t *)request;
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	return uv_shutdown(shutdown, stream, uv_onshutdown);
}
static int tcp_shutdown (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupshutdown, NULL);
}


/* sent [, errmsg] = tcp:send(data [, i [, j]]) */
static void uv_onwritten (uv_write_t *request, int err) {
	lcuU_resumereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupwrite (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_STREAM);
	uv_write_t *write = (uv_write_t *)request;
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	uv_buf_t bufs[1];
	getbufarg(L, bufs);
	return uv_write(write, stream, bufs, 1, uv_onwritten);
}
static int tcp_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupwrite, NULL);
}


/* bytes [, errmsg] = tcp:receive(buffer [, i [, j]]) */
static int stopread (uv_stream_t *stream) {
	int err = uv_read_stop(stream);
	if (err < 0) {
		lua_State *L = (lua_State *)stream->loop->data;
		lcu_closeobj(L, 1, (uv_handle_t *)stream);
	}
	lcu_settcparmed((lcu_TcpSocket *)stream, 0);
	return err;
}
static int k_recvdata (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_STREAM);
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, (uv_handle_t *)stream)) {
		int err = stopread(stream);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	}
	return lua_gettop(L)-4;
}
static int k_getbuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_STREAM);
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcuT_haltedobjop(L, (uv_handle_t *)stream)) {
		uv_buf_t *buf = (uv_buf_t *)lua_touserdata(L, -1);
		lcu_assert(buf);
		lua_pop(L, 1);  /* discard 'buf' */
		getbufarg(L, buf);
		lcuT_awaitobj(L, (uv_handle_t *)stream);
		return lua_yieldk(L, 0, 0, k_recvdata);
	} else {
		int err = stopread(stream);
		if (err < 0) return lcuL_pushresults(L, 0, err);
	}
	return lua_gettop(L)-4;
}
static void uv_onrecvdata (uv_stream_t *stream,
                           ssize_t nread,
                           const uv_buf_t *buf) {
	if (buf->base != (char *)buf) {
		lua_State *thread = (lua_State *)stream->data;
		lcu_assert(thread);
		if (nread >= 0) lua_pushinteger(thread, nread);
		else if (nread != UV_EOF) lcuL_pushresults(thread, 0, nread);
		lcuU_resumeobjop(thread, (uv_handle_t *)stream);
	}
	if (stream->data == NULL) stopread(stream);
}
static int tcp_receive (lua_State *L) {
	lcu_TcpSocket *tcp = ownedtcp(L, lcu_toloop(L), LCU_TCPTYPE_STREAM);
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (stream->data) luaL_argcheck(L, stream->data == L, 1, "already in use");
	else {
		if (!lcu_gettcparmed(tcp)) {
			int err = uv_read_start(stream, uv_ongetbuffer, uv_onrecvdata);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_settcparmed(tcp, 1);
		}
		lcuT_awaitobj(L, (uv_handle_t *)stream);
	}
	lua_settop(L, 4);
	return lua_yieldk(L, 0, 0, k_getbuffer);
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
	else lcu_addtcplisten((lcu_TcpSocket *)stream);
}
static int tcp_listen (lua_State *L) {
	lcu_TcpSocket *tcp = ownedtcp(L, lcu_toloop(L), LCU_TCPTYPE_LISTEN);
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	lua_Integer backlog = luaL_checkinteger(L, 2);
	int err;
	luaL_argcheck(L, !lcu_istcplisten(tcp), 1, "already listening");
	luaL_argcheck(L, 0 <= backlog && backlog <= INT_MAX, 2, "large backlog");
	err = uv_listen(stream, (int)backlog, uv_onconnection);
	if (err >= 0) {
		lcu_marktcplisten(tcp);
		uv_unref((uv_handle_t *)stream); /* uv_listen_stop */
	}
	return lcuL_pushresults(L, 0, err);
}


/* tcp [, errmsg] = socket:accept() */
static int k_accepttcp (lua_State *L, int status, lua_KContext ctx) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPTYPE_LISTEN);
	uv_stream_t *stream = (uv_stream_t *)lcu_totcphandle(tcp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (!lcuT_haltedobjop(L, (uv_handle_t *)stream)) {
		int domain = lcu_gettcpaddrfam(tcp);
		lcu_TcpSocket *newtcp = lcu_newtcp(L, domain, LCU_TCPTYPE_STREAM);
		uv_tcp_t *newhdl = lcu_totcphandle(newtcp);
		int err = uv_tcp_init(stream->loop, newhdl);
		if (err >= 0) {
			err = uv_accept(stream, (uv_stream_t *)newhdl);
			if (err >= 0) lcu_enabletcp(L, -1);
		}
		return lcuL_pushresults(L, 1, err);
	}
	uv_unref((uv_handle_t *)stream);  /* uv_listen_stop */
	return lua_gettop(L)-1;
}
static int tcp_accept (lua_State *L) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPTYPE_LISTEN);
	luaL_argcheck(L, lcu_istcplisten(tcp), 1, "not listening");
	if (!lcu_picktcplisten(tcp)) {
		uv_handle_t *handle = (uv_handle_t *)lcu_totcphandle(tcp);
		luaL_argcheck(L, handle->data == NULL, 1, "already used");
		if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
		uv_ref(handle);  /* uv_listen_start */
		lcuT_awaitobj(L, handle);
		return lua_yieldk(L, 0, 0, k_accepttcp);
	}
	lua_pushlightuserdata(L, loop);  /* token to sign scheduled */
	return k_accepttcp(L, LUA_YIELD, 0);
}


static const luaL_Reg addr[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg udp[] = {
	{"__tostring", udp_tostring},
	{"__gc", udp_gc},
	{"close", udp_close},
	{"getdomain", udp_getdomain},
	{"getaddress", udp_getaddress},
	{"bind", udp_bind},
	{"getoption", udp_getoption},
	{"setoption", udp_setoption},
	{"connect", udp_connect},
	{"send", udp_send},
	{"receive", udp_receive},
	{NULL, NULL}
};

static const luaL_Reg tcp[] = {
	{"__tostring", tcp_tostring},
	{"__gc", tcp_gc},
	{"close", tcp_close},
	{"getdomain", tcp_getdomain},
	{"getaddress", tcp_getaddress},
	{"bind", tcp_bind},
	{NULL, NULL}
};

static const luaL_Reg strm[] = {
	{"getoption", tcp_getoption},
	{"setoption", tcp_setoption},
	{"connect", tcp_connect},
	{"send", tcp_send},
	{"receive", tcp_receive},
	{"shutdown", tcp_shutdown},
	{NULL, NULL}
};

static const luaL_Reg lstn[] = {
	{"listen", tcp_listen},
	{"accept", tcp_accept},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"address", system_address},
	{"socket", system_socket},
	{NULL, NULL}
};

LCULIB_API void lcuM_addtcpf (lua_State *L) {
	lcuM_newclass(L, addr, 0, LCU_NETADDRCLS, NULL);
	lcuM_newclass(L, udp, LCU_MODUPVS, LCU_UDPSOCKCLS, NULL);
	lcuM_newclass(L, tcp, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_SOCKET], NULL);
	lcuM_newclass(L, strm, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_STREAM],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_newclass(L, lstn, LCU_MODUPVS, lcu_TcpSockCls[LCU_TCPTYPE_LISTEN],
	                                    lcu_TcpSockCls[LCU_TCPTYPE_SOCKET]);
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
