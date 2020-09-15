#include "lsyslib.h"
#include "lmodaux.h"
#include "loperaux.h"

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
 * Names
 */

#define tofound(L) ((losi_AddressFound *)luaL_checkudata(L, 1, \
                                         LOSI_NETADDRFOUNDCLS))

#define chkaddrdom(L,I,A,D) luaL_argcheck(L, (A)->sa_family == D, I, "wrong domain")

/* [address, socktype, nextdomain =] found:next([address]) */
static int found_next (lua_State *L) {
	lcu_AddressList *list = lcu_checkaddrlist(L, 1);
	struct addrinfo *found = lcu_peekaddrlist(list);
	if (found) {
		struct sockaddr *addr = lcu_toaddress(L, 2);
		if (addr) {
			chkaddrdom(L, 2, addr, found->ai_family);
			lua_settop(L, 2);
		} else {
			lua_settop(L, 1);
			addr = lcu_newaddress(L, found->ai_family);
		}
		memcpy(addr, found->ai_addr, found->ai_addrlen);
		lua_pushstring(L, (found->ai_socktype == SOCK_DGRAM ? "datagram" :
		                  (found->ai_flags&AI_PASSIVE ? "passive" : "stream" )));
		found = lcu_nextaddrlist(list);
		if (found) pushaddrtype(L, found->ai_family);
		else lua_pushnil(L);
		return 3;
	}
	return 0;
}

static int found_close (lua_State *L) {
	lcu_AddressList *list = lcu_toaddrlist(L, 1);
	if (list) {
		struct addrinfo* results = lcu_getaddrlist(list);
		if (results) {
			freeaddrinfo(results);
			lcu_setaddrlist(list, NULL);
		}
	}
	return 0;
}

/* next, domain = system.resolveaddr (name [, service [, mode]]) */
static void uv_onresolved (uv_getaddrinfo_t *addrreq,
                           int err,
                           struct addrinfo* results) {
	uv_loop_t *loop = addrreq->loop;
	uv_req_t *request = (uv_req_t *)addrreq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lcu_AddressList *list = lcu_newaddrlist(thread);
			lcu_setaddrlist(list, results);
			pushaddrtype(thread, results->ai_family);
			nret = 2;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, nret, loop, request);
	}
	else if (!err) freeaddrinfo(results);
	lcuU_checksuspend(loop);
}
static int k_setupfindaddr (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_getaddrinfo_t *addrreq = (uv_getaddrinfo_t *)request;
	const char *nodename = luaL_optstring(L, 1, NULL);
	const char *servname = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, "");
	struct addrinfo hints;
	int err;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	if (nodename) {
		if (nodename[0] == '*' && nodename[1] == '\0') {
			luaL_argcheck(L, servname, 2, "service must be provided for '*'");
			hints.ai_flags |= AI_PASSIVE;
			nodename = NULL;
		}
	}
	else luaL_argcheck(L, servname, 1, "name or service must be provided");
	for (; *mode; ++mode) switch (*mode) {
		case '4': hints.ai_family = AF_INET; goto aftercase6;
		case '6': hints.ai_family = AF_INET6; aftercase6:
			if (hints.ai_flags&AI_ADDRCONFIG)
				hints.ai_flags &= ~AI_ADDRCONFIG;
			else
				hints.ai_family = AF_UNSPEC;
			break;
		case 's': hints.ai_socktype = SOCK_STREAM; goto aftercased;
		case 'd': hints.ai_socktype = SOCK_DGRAM; aftercased:
			if (hints.ai_protocol)
				hints.ai_socktype = 0;
			else
				hints.ai_protocol = 1;
			break;
		case 'm':
			hints.ai_flags |= AI_V4MAPPED;
			break;
		default:
			return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	if (hints.ai_flags&AI_V4MAPPED) {
		luaL_argcheck(L, hints.ai_family != AF_INET, 3, "'m' is invalid for IPv4");
		if (hints.ai_family == AF_UNSPEC) hints.ai_flags |= AI_ALL;
		hints.ai_family = AF_INET6;
	}
	hints.ai_protocol = 0;  /* clear mark that 'ai_socktype' was defined above */
	err = uv_getaddrinfo(loop, addrreq, uv_onresolved,
	                     nodename, servname, &hints);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_findaddr (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupfindaddr, NULL, NULL);
}

/* name [, service] = system.nameaddr (address [, mode]) */
static void uv_onaddrnamed (uv_getnameinfo_t *namereq,
                            int err,
                            const char *hostname,
                            const char *servname) {
	uv_loop_t *loop = namereq->loop;
	uv_req_t *request = (uv_req_t *)namereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, hostname);
			lua_pushstring(thread, servname);
			nret = 2;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, nret, loop, request);
	}
	lcuU_checksuspend(loop);
}
static void uv_onservnamed (uv_getnameinfo_t *namereq,
                            int err,
                            const char *hostname,
                            const char *servname) {
	uv_loop_t *loop = namereq->loop;
	uv_req_t *request = (uv_req_t *)namereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, servname);
			nret = 1;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, nret, loop, request);
	}
	lcuU_checksuspend(loop);
}
static void uv_oncannonical (uv_getaddrinfo_t *addrreq,
                             int err,
                             struct addrinfo* results) {
	uv_loop_t *loop = addrreq->loop;
	uv_req_t *request = (uv_req_t *)addrreq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, results->ai_canonname);
			nret = 1;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, nret, loop, request);
	}
	freeaddrinfo(results);
}
static int k_setupnameaddr (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	int err;
	int ltype = lua_type(L, 1);
	if (ltype == LUA_TSTRING) {
		uv_getaddrinfo_t *addrreq = (uv_getaddrinfo_t *)request;
		const char *name = lua_tostring(L, 1);
		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_flags = AI_CANONNAME;
		err = uv_getaddrinfo(loop, addrreq, uv_oncannonical, name, NULL, &hints);
	} else {
		uv_getnameinfo_t *namereq = (uv_getnameinfo_t *)request;
		int flags = 0;
		const char *mode = luaL_optstring(L, 2, "");
		for (; *mode; ++mode) switch (*mode) {
			case 'l': flags |= NI_NOFQDN; break;
			case 'd': flags |= NI_DGRAM; break;
#ifdef NI_IDN
			case 'i': flags |= NI_IDN; break;
			case 'u': flags |= NI_IDN|NI_IDN_ALLOW_UNASSIGNED; break;
			case 'a': flags |= NI_IDN|NI_IDN_USE_STD3_ASCII_RULES; break;
#endif
			default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
		}
		if (ltype == LUA_TNUMBER) {
			in_port_t port = int2port(L, lua_tointeger(L, 1), 1);
			struct sockaddr_in addrbuf;
			struct sockaddr *addr = (struct sockaddr *)&addrbuf;
			memset(&addrbuf, 0, sizeof(addrbuf));
			addrbuf.sin_family = AF_INET;
			addrbuf.sin_port = htons(port);
			err = uv_getnameinfo(loop, namereq, uv_onservnamed, addr, flags);
		} else {
			struct sockaddr *addr = lcu_checkaddress(L, 1);
			flags |= NI_NAMEREQD;
			err = uv_getnameinfo(loop, namereq, uv_onaddrnamed, addr, flags);
		}
	}
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_nameaddr (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupnameaddr, NULL, NULL);
}


/*
 * Buffer
 */

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
 * Socket
 */

/* socket [, errmsg] = system.socket(type, domain) */
static int system_socket (lua_State *L) {
	static const char *const SockTypeName[] = { "stream", "passive", "datagram",
	                                            NULL };
	static const char *const SockDomainName[] = { "ipv4", "ipv6", "local", "ipc",
	                                              NULL };
	uv_loop_t *loop = lcu_toloop(L);
	int type = luaL_checkoption(L, 1, NULL, SockTypeName);
	int domain = luaL_checkoption(L, 2, NULL, SockDomainName);
	int err = UV_EAI_SOCKTYPE;
	if (domain < 2) {
		domain = AddrTypeId[domain];
		switch (type) {
			case 0:
			case 1: {
				static const char *const classes[] = { LCU_TCPACTIVECLS,
				                                       LCU_TCPPASSIVECLS };
				lcu_TcpSocket *tcp = lcu_newtcp(L, classes[type], domain);
				err = uv_tcp_init_ex(loop, lcu_totcphdl(tcp), domain);
				if (!err) lcu_enableobj((lcu_Object *)tcp);
			} break;
			case 2: {
				lcu_UdpSocket *udp = lcu_newudp(L, domain);
				err = uv_udp_init_ex(loop, lcu_toudphdl(udp), domain);
				if (!err) lcu_enableobj((lcu_Object *)udp);
			} break;
		}
	} else {
		switch (type) {
			case 0:
			case 1: {
				static const char *const classes[] = { LCU_PIPEACTIVECLS,
				                                       LCU_PIPEPASSIVECLS };
				lcu_IpcPipe *pipe = lcu_newpipe(L, classes[type], domain-2);
				err = uv_pipe_init(loop, lcu_topipehdl(pipe), domain-2);
				if (!err) lcu_enableobj((lcu_Object *)pipe);
			} break;
		}
	}
	return lcuL_pushresults(L, 1, err);
}

#define toclass(L) lua_tostring(L, lua_upvalueindex(LCU_MODUPVS+1))

static lcu_Object *openedobj (lua_State *L, int arg, const char *class) {
	lcu_Object *object = (lcu_Object *)luaL_checkudata(L, arg, class);
	luaL_argcheck(L, !lcu_isobjclosed(object), arg, "closed object");
	return object;
}

static lcu_Object *ownedobj (lua_State *L,
                             uv_loop_t *loop,
                             int arg,
                             const char *class) {
	lcu_Object *object = openedobj(L, arg, class);
	luaL_argcheck(L, lcu_toobjhdl(object)->loop == loop, arg, "foreign object");
	return object;
}


/* getmetatable(object).__gc(object) */
static int object_gc (lua_State *L) {
	lcuT_closeobj(L, 1, toclass(L));
	return 0;
}


/* succ [, errmsg] = object:close() */
static int object_close (lua_State *L) {
	int closed = lcuT_closeobj(L, 1, toclass(L));
	lua_pushboolean(L, closed);
	return 1;
}


/*
 * IP Sockets
 */

static const char *const AddrSites[] = {"this", "peer", NULL};

static struct sockaddr *getaddrarg (lua_State *L, int domain, int *sz) {
	struct sockaddr *addr = lcu_toaddress(L, 3);
	if (addr) {
		lua_settop(L, 3);
		chkaddrdom(L, 3, addr, domain);
	} else {
		lua_settop(L, 2);
		addr = lcu_newaddress(L, domain);
	}
	*sz = (int)lua_rawlen(L, 3);
	return addr;
}

static const struct sockaddr *toobjaddr (lua_State *L,
                                         int idx,
                                         lcu_Object *object) {
	struct sockaddr *address = lcu_checkaddress(L, idx);
	chkaddrdom(L, idx, address, lcu_getobjdomain(object));
	return address;
}

/* domain = ipsock:getdomain() */
static int ipsock_getdomain (lua_State *L) {
	lcu_Object *object = (lcu_Object *)luaL_checkudata(L, 1, toclass(L));
	pushaddrtype(L, lcu_getobjdomain(object));
	return 1;
}

static void completereqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		int nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumereqop(thread, nret, loop, request);
	}
	lcuU_checksuspend(loop);
}

/*
 * UDP
 */

#define openedudp(L)	((lcu_UdpSocket *)openedobj(L, 1, LCU_UDPSOCKETCLS))

#define ownedudp(L,l)	((lcu_UdpSocket *)ownedobj(L, l, 1, LCU_UDPSOCKETCLS))


/* address [, errmsg] = udp:getaddress([site [, address]]) */
static int udp_getaddress (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphdl(udp);
	int peer = luaL_checkoption(L, 2, "this", AddrSites);
	int domain = lcu_getobjdomain((lcu_Object *)udp);
	int addrsz;
	struct sockaddr *addr = getaddrarg(L, domain, &addrsz);
	int err = peer ? uv_udp_getpeername(handle, addr, &addrsz)
	               : uv_udp_getsockname(handle, addr, &addrsz);
	lcu_assert(addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = udp:bind(address) */
static int udp_bind (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	const struct sockaddr *addr = toobjaddr(L, 2, (lcu_Object *)udp);
	int err = uv_udp_bind(lcu_toudphdl(udp), addr, 0);
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
	uv_udp_t *handle = lcu_toudphdl(udp);
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
			int domain = lcu_getobjdomain((lcu_Object *)udp);
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
	const struct sockaddr *addr = lua_isnil(L, 2) ?
	                              NULL : toobjaddr(L, 2, (lcu_Object *)udp);
	int err = uv_udp_connect(lcu_toudphdl(udp), addr);
	if (err >= 0) lcu_setudpconnected(udp, addr != NULL);
	return lcuL_pushresults(L, 0, err);
}



/* sent [, errmsg] = udp:send(data [, i [, j [, address]]]) */
static void uv_onsent (uv_udp_send_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupsend (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_UdpSocket *udp = ownedudp(L, loop);
	uv_udp_send_t *send = (uv_udp_send_t *)request;
	uv_udp_t *handle = (uv_udp_t *)lcu_toudphdl(udp);
	uv_buf_t bufs[1];
	const struct sockaddr *addr = lcu_getudpconnected(udp) ?
	                              NULL : toobjaddr(L, 5, (lcu_Object *)udp);
	int err;
	getbufarg(L, bufs);
	err = uv_udp_send(send, handle, bufs, 1, addr, uv_onsent);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int udp_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupsend, NULL, NULL);
}


/* bytes [, errmsg] = udp:receive(buffer [, i [, j [, address]]]) */
static void stopudp (uv_udp_t *udp) {
	int err = uv_udp_recv_stop(udp);
	if (err < 0) {
		lua_State *L = (lua_State *)udp->loop->data;
		lcuT_closeobjhdl(L, 1, (uv_handle_t *)udp);
		lcuL_warnerror(L, "datagram:receive: ", err);
	}
	lcu_setobjarmed(lcu_tohdlobj((uv_handle_t *)udp), 0);
}
static int k_udpbuffer (lua_State *L, int status, lua_KContext ctx);
static int k_udprecv (lua_State *L, int status, lua_KContext ctx) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_toudphdl(udp);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, (uv_handle_t *)handle)) {
		stopudp(handle);
	} else if (lua_isinteger(L, 6)) {
		const struct sockaddr *src = (const struct sockaddr *)lua_touserdata(L, -1);
		lua_pop(L, 1);  /* discard 'addr' lightuserdata */
		if (src) {
			struct sockaddr *dst = lcu_toaddress(L, 5);
			if (dst) {
				lcu_assert(src->sa_family == lcu_getobjdomain((lcu_Object *)udp));
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
	uv_udp_t *handle = lcu_toudphdl(udp);
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
		stopudp(handle);
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
		int nret = 0;
		lcu_assert(thread);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			lua_pushboolean(thread, flags&UV_UDP_PARTIAL);
			lua_pushlightuserdata(thread, (void *)addr);
			nret = 3;
		}
		else if (nread != UV_EOF) nret = lcuL_pushresults(thread, 0, nread);
		lcuU_resumeobjop(thread, nret, (uv_handle_t *)udp);
	}
	if (udp->data == NULL) stopudp(udp);
	lcuU_checksuspend(udp->loop);
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
		lcuU_resumeobjop(thread, 1, handle);
	} while (buf->base == (char *)buf && handle->data);
}
static int udp_receive (lua_State *L) {
	lcu_UdpSocket *udp = ownedudp(L, lcu_toloop(L));
	uv_udp_t *handle = lcu_toudphdl(udp);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (handle->data) luaL_argcheck(L, handle->data == L, 1, "already in use");
	else {
		lcu_Object *obj = (lcu_Object *)udp;
		if (!lcu_getobjarmed(obj)) {
			int err = uv_udp_recv_start(handle, uv_ongetbuffer, uv_onudprecv);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_setobjarmed(obj, 1);
		}
		lcuT_awaitobj(L, (uv_handle_t *)handle);
	}
	lua_settop(L, 5);
	return lua_yieldk(L, 0, 0, k_udpbuffer);
}


/*
 * Stream
 */

/* succ [, errmsg] = stream:shutdown() */
static void uv_onshutdown (uv_shutdown_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupshutdown (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_Object *object = ownedobj(L, loop, 1, toclass(L));
	uv_stream_t *stream = (uv_stream_t *)lcu_toobjhdl(object);
	uv_shutdown_t *shutdown = (uv_shutdown_t *)request;
	int err = uv_shutdown(shutdown, stream, uv_onshutdown);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int active_shutdown (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupshutdown, NULL, NULL);
}


/* sent [, errmsg] = stream:send(data [, i [, j]]) */
static void uv_onwritten (uv_write_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupwrite (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_Object *object = ownedobj(L, loop, 1, toclass(L));
	uv_stream_t *stream = (uv_stream_t *)lcu_toobjhdl(object);
	uv_write_t *write = (uv_write_t *)request;
	uv_buf_t bufs[1];
	int err;
	getbufarg(L, bufs);
	err = uv_write(write, stream, bufs, 1, uv_onwritten);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int active_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupwrite, NULL, NULL);
}


/* sent [, errmsg] = pipe:send(data [, i [, j [, object]]]) */
static int k_setupwriteobj (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_Object *object = ownedobj(L, loop, 1, LCU_PIPEIPCCLS);
	uv_stream_t *stream = (uv_stream_t *)lcu_toobjhdl(object);
	uv_write_t *write = (uv_write_t *)request;
	uv_buf_t bufs[1];
	uv_stream_t *wrtstrm = NULL;
	int err;
	if (lua_isuserdata(L, 5)) {
		if (lua_getmetatable(L, 5)) {  /* does it have a metatable? */
			static const char *const classes[] = { LCU_TCPACTIVECLS,
			                                       LCU_TCPPASSIVECLS,
			                                       LCU_PIPEACTIVECLS,
			                                       LCU_PIPEPASSIVECLS,
			                                       NULL };
			int i;
			for (i = 0; classes[i]; ++i) {
				int samemt = 0;
				luaL_getmetatable(L, classes[i]);  /* get correct metatable */
				samemt = lua_rawequal(L, -1, -2);
				lua_pop(L, 1);  /* remove object metatables */
				if (samemt) {
					lcu_Object *wrtobj = (lcu_Object *)lua_touserdata(L, 5);
					wrtstrm = (uv_stream_t *)lcu_toobjhdl(wrtobj);
					break;
				}
			}
			lua_pop(L, 1);  /* remove expected metatables */
		}
		luaL_argcheck(L, wrtstrm, 5, "writable object expected");
	}
	getbufarg(L, bufs);
	err = uv_write2(write, stream, bufs, 1, wrtstrm, uv_onwritten);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int pipe_send (lua_State *L) {
	return lcuT_resetreqopk(L, k_setupwriteobj, NULL, NULL);
}

/* bytes [, errmsg] = stream:receive(buffer [, i [, j]]) */
static void stopread (uv_stream_t *stream) {
	int err = uv_read_stop(stream);
	if (err < 0) {
		lua_State *L = (lua_State *)stream->loop->data;
		lcuT_closeobjhdl(L, 1, (uv_handle_t *)stream);
		lcuL_warnerror(L, "stream:receive: ", err);
	}
	lcu_setobjarmed(lcu_tohdlobj((uv_handle_t *)stream), 0);
}
static int k_recvdata (lua_State *L, int status, lua_KContext ctx) {
	lcu_Object *object = lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_toobjhdl(object);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, handle)) stopread((uv_stream_t *)handle);
	return lua_gettop(L)-4;
}
static int k_getbuffer (lua_State *L, int status, lua_KContext ctx) {
	lcu_Object *object = lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_toobjhdl(object);
	lua_KFunction cont = (lua_KFunction)ctx;
	lcu_assert(status == LUA_YIELD);
	if (!lcuT_haltedobjop(L, handle)) {
		uv_buf_t *buf = (uv_buf_t *)lua_touserdata(L, -1);
		lcu_assert(buf);
		lua_pop(L, 1);  /* discard 'buf' */
		getbufarg(L, buf);
		lcuT_awaitobj(L, handle);
		return lua_yieldk(L, 0, 0, cont);
	}
	else stopread((uv_stream_t *)handle);
	return lua_gettop(L)-4;
}
static void uv_onrecvdata (uv_stream_t *stream,
                           ssize_t nread,
                           const uv_buf_t *buf) {
	if (buf->base != (char *)buf) {
		lua_State *thread = (lua_State *)stream->data;
		int nret = 0;
		lcu_assert(thread);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			nret = 1;
		}
		else if (nread != UV_EOF) nret = lcuL_pushresults(thread, 0, nread);
		lcuU_resumeobjop(thread, nret, (uv_handle_t *)stream);
	}
	if (stream->data == NULL) stopread(stream);
	lcuU_checksuspend(stream->loop);
}
static int active_receive (lua_State *L) {
	lcu_Object *object = ownedobj(L, lcu_toloop(L), 1, toclass(L));
	uv_handle_t *handle = lcu_toobjhdl(object);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (handle->data) luaL_argcheck(L, handle->data == L, 1, "already in use");
	else {
		if (!lcu_getobjarmed(object)) {
			uv_stream_t *stream = (uv_stream_t *)handle;
			int err = uv_read_start(stream, uv_ongetbuffer, uv_onrecvdata);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_setobjarmed(object, 1);
		}
		lcuT_awaitobj(L, handle);
	}
	lua_settop(L, 4);
	return lua_yieldk(L, 0, (lua_KContext)k_recvdata, k_getbuffer);
}


/* bytes [, errmsg] = stream:receive(buffer [, i [, j]]) */
static int pushstreamread (lua_State *L, uv_pipe_t *pipe) {
	uv_handle_type type = uv_pipe_pending_type(pipe);
	lcu_Object *object;
	uv_stream_t *stream;
	int err;
	switch (type) {
		case UV_NAMED_PIPE: {
			lcu_IpcPipe *newpipe = lcu_newpipe(L, LCU_PIPEACTIVECLS, 0);
			uv_pipe_t *handle = lcu_topipehdl(newpipe);
			err = uv_pipe_init(pipe->loop, handle, 0);
			object = (lcu_Object*)newpipe;
			stream = (uv_stream_t *)handle;
		} break;
		case UV_TCP: {
			lcu_TcpSocket *tcp = lcu_newtcp(L, LCU_TCPACTIVECLS, AF_UNSPEC);
			uv_tcp_t *handle = (uv_tcp_t *)lcu_totcphdl(tcp);
			err = uv_tcp_init(pipe->loop, handle);
			object = (lcu_Object*)tcp;
			stream = (uv_stream_t *)handle;
		} break;
		default: return UV_EAI_SOCKTYPE;
	}
	if (err) return err;
	lcu_enableobj(object);
	return uv_accept((uv_stream_t *)pipe, stream);
}
static int k_recvpipedata (lua_State *L, int status, lua_KContext ctx) {
	lcu_Object *object = lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_toobjhdl(object);
	lcu_assert(status == LUA_YIELD);
	lcu_assert(!ctx);
	if (lcuT_haltedobjop(L, handle)) {
		stopread((uv_stream_t *)handle);
	} else {
		uv_pipe_t *pipe = (uv_pipe_t *)handle;
		if (uv_pipe_pending_count(pipe)) {  /* only if read was successful? */
			int err = pushstreamread(L, pipe);
			return lcuL_pushresults(L, lua_gettop(L)-4, err);
		}
	}
	return lua_gettop(L)-4;
}
static int pipe_receive (lua_State *L) {
	lcu_Object *object = ownedobj(L, lcu_toloop(L), 1, LCU_PIPEIPCCLS);
	uv_handle_t *handle = lcu_toobjhdl(object);
	uv_pipe_t *pipe = (uv_pipe_t *)handle;
	if (uv_pipe_pending_count(pipe)) {
		int err;
		lua_pushinteger(L, 0);  /* byte count should be zero */
		err = pushstreamread(L, pipe);
		return lcuL_pushresults(L, 2, err);
	}
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (handle->data) luaL_argcheck(L, handle->data == L, 1, "already in use");
	else {
		if (!lcu_getobjarmed(object)) {
			uv_stream_t *stream = (uv_stream_t *)handle;
			int err = uv_read_start(stream, uv_ongetbuffer, uv_onrecvdata);
			if (err < 0) return lcuL_pushresults(L, 0, err);
			lcu_setobjarmed(object, 1);
		}
		lcuT_awaitobj(L, handle);
	}
	lua_settop(L, 4);
	return lua_yieldk(L, 0, (lua_KContext)k_recvpipedata, k_getbuffer);
}


/* succ [, errmsg] = listen:listen(backlog) */
static void uv_onconnection (uv_stream_t *stream, int status) {
	uv_handle_t *handle = (uv_handle_t *)stream;
	lua_State *thread = (lua_State *)stream->data;
	if (thread) {
		lua_pushinteger(thread, status);
		lcuU_resumeobjop(thread, 1, handle);
		if (stream->data == NULL) uv_unref(handle);
		else lcu_assert(uv_has_ref(handle));
	}
	else lcu_addobjlisten(lcu_tohdlobj(handle));
	lcuU_checksuspend(stream->loop);
}
static int passive_listen (lua_State *L) {
	lcu_Object *object = ownedobj(L, lcu_toloop(L), 1, toclass(L));
	uv_handle_t *handle = lcu_toobjhdl(object);
	lua_Integer backlog = luaL_checkinteger(L, 2);
	int err;
	luaL_argcheck(L, !lcu_isobjlisten(object), 1, "already listening");
	luaL_argcheck(L, 0 <= backlog && backlog <= INT_MAX, 2, "large backlog");
	err = uv_listen((uv_stream_t *)handle, (int)backlog, uv_onconnection);
	if (err >= 0) {
		lcu_markobjlisten(object);
		uv_unref(handle); /* uv_listen_stop */
	}
	return lcuL_pushresults(L, 0, err);
}


typedef int (*NewAcceptFunc) (lua_State *L,
                              uv_loop_t *loop,
                              lcu_Object *object,
                              lcu_Object **newobj);

/* stream [, errmsg] = listen:accept() */
static int k_acceptstream (lua_State *L, int status, lua_KContext ctx) {
	lcu_Object *object = (lcu_Object *)lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_toobjhdl(object);
	lcu_assert(status == LUA_YIELD);
	if (!lcuT_haltedobjop(L, handle)) {
		NewAcceptFunc newaccept = (NewAcceptFunc)ctx;
		lcu_Object *newobj;
		int err = newaccept(L, handle->loop, object, &newobj);
		if (err >= 0) {
			lcu_enableobj(newobj);
			err = uv_accept((uv_stream_t *)handle,
			                (uv_stream_t *)lcu_toobjhdl(newobj));
		}
		else lcu_addobjlisten(object);
		return lcuL_pushresults(L, 1, err);
	}
	uv_unref(handle);  /* uv_listen_stop */
	return lua_gettop(L)-1;
}
static int listen_accept (lua_State *L,
                          const char *class,
                          NewAcceptFunc newaccept) {
	uv_loop_t *loop = lcu_toloop(L);
	lcu_Object *object = ownedobj(L, loop, 1, class);
	luaL_argcheck(L, lcu_isobjlisten(object), 1, "not listening");
	if (!lcu_pickobjlisten(object)) {
		uv_handle_t *handle = lcu_toobjhdl(object);
		luaL_argcheck(L, handle->data == NULL, 1, "already used");
		if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
		uv_ref(handle);  /* uv_listen_start */
		lcuT_awaitobj(L, handle);
		return lua_yieldk(L, 0, (lua_KContext)newaccept, k_acceptstream);
	}
	lua_pushlightuserdata(L, loop);  /* token to sign scheduled */
	return k_acceptstream(L, LUA_YIELD, (lua_KContext)newaccept);
}


/*
 * TCP
 */

#define openedtcp(L,c)	((lcu_TcpSocket *)openedobj(L, 1, c))

#define ownedtcp(L,l,c)	((lcu_TcpSocket *)ownedobj(L, l, 1, c))


/* address [, errmsg] = tcp:getaddress([site [, address]]) */
static int tcp_getaddress (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, toclass(L));
	uv_tcp_t *handle = lcu_totcphdl(tcp);
	int peer = luaL_checkoption(L, 2, "this", AddrSites);
	int domain = lcu_getobjdomain((lcu_Object *)tcp);
	int addrsz;
	struct sockaddr *addr = getaddrarg(L, domain, &addrsz);
	int err = peer ? uv_tcp_getpeername(handle, addr, &addrsz)
	               : uv_tcp_getsockname(handle, addr, &addrsz);
	lcu_assert(addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = tcp:bind(address) */
static int tcp_bind (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, toclass(L));
	const struct sockaddr *addr = toobjaddr(L, 2, (lcu_Object *)tcp);
	int err = uv_tcp_bind(lcu_totcphdl(tcp), addr, 0);
	return lcuL_pushresults(L, 0, err);
}


static const char * const TcpOptions[] = {"keepalive", "nodelay", NULL};

/* succ [, errmsg] = tcp:setoption(name, value) */
static int tcp_setoption (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPACTIVECLS);
	int opt = luaL_checkoption(L, 2, NULL, TcpOptions);
	int err, enabled = lua_toboolean(L, 3);
	luaL_checkany(L, 3);
	switch (opt) {
		case 0: {  /* keepalive */
			int delay = -1;
			if (enabled) {
				delay = (int)luaL_checkinteger(L, 3);
				luaL_argcheck(L, delay > 0, 3, "invalid argument");
			}
			err = uv_tcp_keepalive(lcu_totcphdl(tcp), enabled, delay);
			if (err >= 0) lcu_settcpkeepalive(tcp, delay);
		}; break;
		case 1: {  /* nodelay */
			err = uv_tcp_nodelay(lcu_totcphdl(tcp), enabled);
			if (err >= 0) lcu_settcpnodelay(tcp, enabled);
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}

/* value = tcp:getoption(name) */
static int tcp_getoption (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPACTIVECLS);
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
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setuptcpconn (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_TcpSocket *tcp = ownedtcp(L, loop, LCU_TCPACTIVECLS);
	const struct sockaddr *addr = toobjaddr(L, 2, (lcu_Object *)tcp);
	uv_connect_t *connect = (uv_connect_t *)request;
	int err = uv_tcp_connect(connect, lcu_totcphdl(tcp), addr, uv_onconnected);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int tcp_connect (lua_State *L) {
	return lcuT_resetreqopk(L, k_setuptcpconn, NULL, NULL);
}


/* tcp [, errmsg] = tcp:accept() */
static int newtcpaccept (lua_State *L,
                         uv_loop_t *loop,
                         lcu_Object *object,
                         lcu_Object **newobj) {
	int domain = lcu_getobjdomain(object);
	*newobj = (lcu_Object *)lcu_newtcp(L, LCU_TCPACTIVECLS, domain);
	return uv_tcp_init(loop, (uv_tcp_t *)lcu_toobjhdl(*newobj));
}
static int tcp_accept (lua_State *L) {
	return listen_accept(L, LCU_TCPPASSIVECLS, newtcpaccept);
}


/*
 * Pipe
 */

#define openedpipe(L,c)	((lcu_IpcPipe *)openedobj(L, 1, c))

#define ownedpipe(L,l,c)	((lcu_IpcPipe *)ownedobj(L, l, 1, c))


/* address [, errmsg] = pipe:getaddress([site]) */
static int pipe_getaddress (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L, toclass(L));
	uv_pipe_t *handle = lcu_topipehdl(pipe);
	int peer = luaL_checkoption(L, 2, "this", AddrSites);
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
		buf = (char *)lua_newuserdatauv(L, bufsz, 0);
		goto again;
	}
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = pipe:bind(address) */
static int pipe_bind (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L, toclass(L));
	const char *addr = luaL_checkstring(L, 2);
	int err = uv_pipe_bind(lcu_topipehdl(pipe), addr);
	return lcuL_pushresults(L, 0, err);
}


static const char * const PipeOptions[] = {"permission", NULL};

/* succ [, errmsg] = pipe:setoption(name, value) */
static int pipe_setoption (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L, toclass(L));
	uv_pipe_t *handle = lcu_topipehdl(pipe);
	int opt = luaL_checkoption(L, 2, NULL, PipeOptions);
	int err;
	switch (opt) {
		case 0: {  /* permission */
			const char *mode = luaL_optstring(L, 3, "");
			int flags;
			for (; *mode; ++mode) switch (*mode) {
				case 'r': flags |= UV_READABLE; break;
				case 'w': flags |= UV_WRITABLE; break;
				default: return luaL_error(L, "unknown option (got '%c')", *mode);
			}
			err = uv_pipe_chmod(handle, flags);
			if (!err) lcu_setpipeperm(pipe, flags);
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}

/* value = pipe:getoption(name) */
static int pipe_getoption (lua_State *L) {
	lcu_IpcPipe *pipe = openedpipe(L, toclass(L));
	int opt = luaL_checkoption(L, 2, NULL, PipeOptions);
	switch (opt) {
		case 0: {  /* permission */
			int flags = lcu_getpipeperm(pipe);
			switch (flags) {
				case UV_READABLE: lua_pushliteral(L, "r");
				case UV_WRITABLE: lua_pushliteral(L, "w");
				case UV_READABLE|UV_WRITABLE: lua_pushliteral(L, "rw");
				default: lua_pushliteral(L, "");
			}
		}; break;
		default: return 0;
	}
	return 1;
}


/* succ [, errmsg] = pipe:connect(address) */
static int k_setuppipeconn (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	lcu_IpcPipe *pipe = ownedpipe(L, loop, toclass(L));
	const char *addr = luaL_checkstring(L, 2);
	uv_connect_t *connect = (uv_connect_t *)request;
	uv_pipe_connect(connect, lcu_topipehdl(pipe), addr, uv_onconnected);
	return -1;  /* yield on success */
}
static int pipe_connect (lua_State *L) {
	return lcuT_resetreqopk(L, k_setuppipeconn, NULL, NULL);
}


static int newpipeaccept (lua_State *L,
                          uv_loop_t *loop,
                          lcu_Object *object,
                          lcu_Object **newobj) {
	int ipc = lcu_getobjdomain(object) == AF_INET6;
	*newobj = (lcu_Object *)lcu_newpipe(L, LCU_PIPEACTIVECLS, ipc);
	return uv_pipe_init(loop, (uv_pipe_t *)lcu_toobjhdl(*newobj), ipc);
}
static int pipe_accept (lua_State *L) {
	return listen_accept(L, LCU_PIPEPASSIVECLS, newpipeaccept);
}


/*
 * Module
 */

static const luaL_Reg addr[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg found[] = {
	{"__gc", found_close},
	{"__call", found_next},
	{NULL, NULL}
};

static const luaL_Reg object[] = {
	{"__gc", object_gc},
	{"close", object_close},
	{NULL, NULL}
};

static const luaL_Reg ip[] = {
	{"getdomain", ipsock_getdomain},
	{NULL, NULL}
};

static const luaL_Reg udp[] = {
	{"getaddress", udp_getaddress},
	{"bind", udp_bind},
	{"getoption", udp_getoption},
	{"setoption", udp_setoption},
	{"connect", udp_connect},
	{"send", udp_send},
	{"receive", udp_receive},
	{NULL, NULL}
};

static const luaL_Reg active[] = {
	{"shutdown", active_shutdown},
	{"send", active_send},
	{"receive", active_receive},
	{NULL, NULL}
};

static const luaL_Reg passive[] = {
	{"listen", passive_listen},
	{NULL, NULL}
};

static const luaL_Reg tcp[] = {
	{"getaddress", tcp_getaddress},
	{"bind", tcp_bind},
	{NULL, NULL}
};

static const luaL_Reg tcpactive[] = {
	{"getoption", tcp_getoption},
	{"setoption", tcp_setoption},
	{"connect", tcp_connect},
	{NULL, NULL}
};

static const luaL_Reg tcppassive[] = {
	{"accept", tcp_accept},
	{NULL, NULL}
};

static const luaL_Reg pipe[] = {
	{"getaddress", pipe_getaddress},
	{"bind", pipe_bind},
	{"setoption", pipe_setoption},
	{"getoption", pipe_getoption},
	{NULL, NULL}
};

static const luaL_Reg pipeactive[] = {
	{"connect", pipe_connect},
	{NULL, NULL}
};

static const luaL_Reg pipepassive[] = {
	{"accept", pipe_accept},
	{NULL, NULL}
};

static const luaL_Reg pipeipc[] = {
	{"send", pipe_send},
	{"receive", pipe_receive},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"address", system_address},
	{"findaddr", system_findaddr},
	{"nameaddr", system_nameaddr},
	{"socket", system_socket},
	{NULL, NULL}
};

LCUI_FUNC void lcuM_addcommunc (lua_State *L) {
	lcuM_newclass(L, LCU_NETADDRCLS);
	lcuM_setfuncs(L, addr, 0);
	lua_pop(L, 1);

	lcuM_newclass(L, LCU_NETADDRLISTCLS);
	lcuM_setfuncs(L, found, 0);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_UDPSOCKETCLS);
	lcuM_newclass(L, LCU_UDPSOCKETCLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, ip, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lcuM_setfuncs(L, udp, LCU_MODUPVS);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_TCPACTIVECLS);
	lcuM_newclass(L, LCU_TCPACTIVECLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, ip, LCU_MODUPVS+1);
	lcuM_setfuncs(L, tcp, LCU_MODUPVS+1);
	lcuM_setfuncs(L, active, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lcuM_setfuncs(L, tcpactive, LCU_MODUPVS);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_TCPPASSIVECLS);
	lcuM_newclass(L, LCU_TCPPASSIVECLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, ip, LCU_MODUPVS+1);
	lcuM_setfuncs(L, tcp, LCU_MODUPVS+1);
	lcuM_setfuncs(L, passive, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lcuM_setfuncs(L, tcppassive, LCU_MODUPVS);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_PIPEIPCCLS);
	lcuM_newclass(L, LCU_PIPEIPCCLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, pipe, LCU_MODUPVS+1);
	lcuM_setfuncs(L, active, LCU_MODUPVS+1);
	lcuM_setfuncs(L, pipeactive, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lcuM_setfuncs(L, pipeipc, LCU_MODUPVS);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_PIPEACTIVECLS);
	lcuM_newclass(L, LCU_PIPEACTIVECLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, pipe, LCU_MODUPVS+1);
	lcuM_setfuncs(L, active, LCU_MODUPVS+1);
	lcuM_setfuncs(L, pipeactive, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lua_pop(L, 1);

	lua_pushstring(L, LCU_PIPEPASSIVECLS);
	lcuM_newclass(L, LCU_PIPEPASSIVECLS);
	lcuM_setfuncs(L, object, LCU_MODUPVS+1);
	lcuM_setfuncs(L, pipe, LCU_MODUPVS+1);
	lcuM_setfuncs(L, passive, LCU_MODUPVS+1);
	lua_remove(L, -2);
	lcuM_setfuncs(L, pipepassive, LCU_MODUPVS);
	lua_pop(L, 1);
}

LCUI_FUNC void lcuM_addcommunf (lua_State *L) {
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
