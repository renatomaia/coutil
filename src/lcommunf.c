#include "lmodaux.h"
#include "loperaux.h"
#include "lsckdefs.h"
#include "lttyaux.h"

#include <string.h>
#include <luamem.h>


#if !LCU_LIBUVMINVER(3)
#define uv_pipe_getpeername(H,B,S)	((void)(H),(void)(B),(void)(S),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(16)
#define uv_pipe_chmod(H,F)	((void)(H),(void)(F),UV_ENOSYS)
#endif


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


#define checknetaddr(L,i)  ((struct sockaddr *) \
                           luaL_checkudata(L, i, LCU_NETADDRCLS))

#define tonetaddr(L,i)  ((struct sockaddr *) \
                        luaL_testudata(L, i, LCU_NETADDRCLS))


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


static struct sockaddr *newaddress (lua_State *L, int type) {
	struct sockaddr *na;
	size_t sz;
	switch (type) {
		case AF_INET: sz = sizeof(struct sockaddr_in); break;
		case AF_INET6: sz = sizeof(struct sockaddr_in6); break;
		default: luaL_error(L, "invalid address type"); return NULL;
	}
	na = (struct sockaddr *)lua_newuserdatauv(L, sz, 0);
	memset(na, 0, sz);
	luaL_setmetatable(L, LCU_NETADDRCLS);
	return na;
}


/* address [, errmsg] = system.address(type, [data [, port [, format]]]) */
static int system_address (lua_State *L) {
	int type = AddrTypeId[luaL_checkoption(L, 1, NULL, AddrTypeName)];
	int n = lua_gettop(L);
	struct sockaddr *na;
	lua_settop(L, 4);
	na = newaddress(L, type);
	luaL_setmetatable(L, LCU_NETADDRCLS);
	lcu_assert(na);
	na->sa_family = type;
	if (n > 1) {
		in_port_t port = 0;
		size_t sz;
		const char *data = luamem_checkarray(L, 2, &sz);
		if (n == 2) {  /* URI format */
			int err;
			char literal[LCU_ADDRMAXLITERAL];
			const char *c, *e = data+sz;
			switch (type) {
				case AF_INET: {
					c = memchr(data, ':', sz-1);  /* at least one port digit */
					luaL_argcheck(L, c, 2, "invalid URI format");
					sz = c-data;
				} break;
				case AF_INET6: {
					c = memchr(++data, ']', sz-3);  /* intial '[' and final ':?' */
					luaL_argcheck(L, c && c[1] == ':', 2, "invalid URI format");
					sz = (c++)-data;
				} break;
				default: return lcu_error(L, UV_EAFNOSUPPORT);
			}
			luaL_argcheck(L, sz <= LCU_ADDRMAXLITERAL, 2, "invalid URI format");
			luaL_argcheck(L, mem2port(c+1, e, &port), 2, "invalid port");
			memcpy(literal, data, sz);
			literal[sz] = '\0';
			err = setaddrliteral(na, literal);
			if (err) return lcu_error(L, err);
		} else {
			const char *mode = luaL_optstring(L, 4, "t");
			port = int2port(L, luaL_checkinteger(L, 3), 3);
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
	struct sockaddr *na = checknetaddr(L, 1);
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
	struct sockaddr *a1 = tonetaddr(L, 1);
	struct sockaddr *a2 = tonetaddr(L, 2);
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
	struct sockaddr *na = checknetaddr(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* port */
			lua_pushinteger(L, getaddrport(na));
		} break;
		case 1: {  /* binary */
			size_t sz;
			const char *s = getaddrbytes(na, &sz);
			lua_pushlstring(L, s, sz);
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
	struct sockaddr *na = checknetaddr(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* port */
			setaddrport(na, int2port(L, luaL_checkinteger(L,3), 3));
		} break;
		case 1: {  /* binary */
			size_t sz;
			const char *data = luamem_checkarray(L, 3, &sz);
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

#define chkaddrdom(L,I,A,D) luaL_argcheck(L, (A)->sa_family == D, I, \
                                             "wrong domain")

typedef struct AddressList {
	struct addrinfo *start;
	struct addrinfo *current;
} AddressList;

#define checkaddrlist(L)	(AddressList *) luaL_checkudata(L, 1, LCU_NETADDRLISTCLS)

static AddressList *openedaddrlist (lua_State *L) {
	AddressList *list = checkaddrlist(L);
	luaL_argcheck(L, list->start, 1, "closed "LCU_NETADDRLISTCLS);
	return list;
}

static struct addrinfo *findvalidaddr (struct addrinfo *addr) {
	for (; addr; addr = addr->ai_next) {
		switch (addr->ai_socktype) {
#ifdef _WIN32
			case 0:
#endif
			case SOCK_DGRAM:
			case SOCK_STREAM: return addr;
		}
	}
	return NULL;
}

static void closeaddrlist (AddressList *list) {
	struct addrinfo* results = list->start;
	if (results) {
		uv_freeaddrinfo(results);
		list->start = NULL;
		list->current = NULL;
	}
}

static int found_gc (lua_State *L) {
	AddressList *list = checkaddrlist(L);
	closeaddrlist(list);
	return 0;
}

/* true = found:close() */
static int found_close (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	closeaddrlist(list);
	lua_pushboolean(L, 1);
	return 1;
}

/* changed = found:next() */
static int found_next (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	struct addrinfo *found = findvalidaddr(list->current->ai_next);
	if (found) {
		list->current = found;
		lua_pushboolean(L, 1);
	}
	else lua_pushboolean(L, 0);
	return 1;
}

/* found:reset() */
static int found_reset (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	list->current = findvalidaddr(list->start);
	lcu_assert(list->current);
	return 0;
}

/* address = found:getaddress([address]) */
static int found_getaddress (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	struct addrinfo *found = list->current;
	struct sockaddr *addr = tonetaddr(L, 2);
	if (addr) {
		chkaddrdom(L, 2, addr, found->ai_family);
		lua_settop(L, 2);
	} else {
		lua_settop(L, 1);
		addr = newaddress(L, found->ai_family);
	}
	memcpy(addr, found->ai_addr, found->ai_addrlen);
	return 1;
}

/* domain = found:getdomain() */
static int found_getdomain (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	struct addrinfo *found = list->current;
	pushaddrtype(L, found->ai_family);
	return 1;
}

/* type = found:getsocktype() */
static int found_getsocktype (lua_State *L) {
	AddressList *list = openedaddrlist(L);
	struct addrinfo *found = list->current;
	switch (found->ai_socktype) {
		case SOCK_DGRAM: {
			lua_pushliteral(L, "datagram");
		} break;
		case SOCK_STREAM: {
			if (found->ai_flags&AI_PASSIVE) lua_pushliteral(L, "passive");
			else lua_pushliteral(L, "stream");
		} break;
		default: lua_pushnil(L);
	}
	return 1;
}

/* next, domain = system.findaddr (name [, service [, mode]]) */
static int returnfound (lua_State *L) {
	AddressList *list = (AddressList *)lua_touserdata(L, 3);
	int err = lua_tointeger(L, 4);
	struct addrinfo *results = (struct addrinfo *)lua_touserdata(L, 5);
	lua_settop(L, 3);
	if (!err) {
		struct addrinfo* found = findvalidaddr(results);
		if (found) {
			list->start = results;
			list->current = found;
			luaL_setmetatable(L, LCU_NETADDRLISTCLS);
			return 1;
		}
		uv_freeaddrinfo(results);
		err = UV_EAFNOSUPPORT;
	}
	return lcuL_pusherrres(L, err);
}
static void uv_onresolved (uv_getaddrinfo_t *addrreq,
                           int err,
                           struct addrinfo *results) {
	uv_loop_t *loop = addrreq->loop;
	uv_req_t *request = (uv_req_t *)addrreq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	if (thread) {
		lua_pushinteger(thread, err);
		lua_pushlightuserdata(thread, results);
		lcuU_resumecoreq(loop, request, 2);
	}
	else if (!err) uv_freeaddrinfo(results);
}
static int k_setupfindaddr (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	uv_getaddrinfo_t *addrreq = (uv_getaddrinfo_t *)request;
	const char *nodename = luaL_optstring(L, 1, NULL);
	const char *servname = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, "");
	struct addrinfo hints;
	int res;
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
	for (; *mode; mode++) switch (*mode) {
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
		case 'n':
			hints.ai_flags |= AI_NUMERICHOST;
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
	lua_settop(L, 2);
	lua_tointegerx(L, 2, &res);  /* check if servname is numeric */
	if (res) hints.ai_flags |= AI_NUMERICSERV;
	lua_newuserdatauv(L, sizeof(AddressList), 0);  /* raise memory errors */
	res = uv_getaddrinfo(loop, addrreq, uv_onresolved, nodename, servname, &hints);
	lcuT_armcoreq(L, loop, op, res);
	if (res < 0) return lcuL_pusherrres(L, res);
	return -1;  /* yield on success */
}
static int system_findaddr (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetcoreqk(L, sched, k_setupfindaddr, returnfound, NULL);
}

/* name [, service] = system.nameaddr (address [, mode]) */
static void uv_onaddrnamed (uv_getnameinfo_t *namereq,
                            int err,
                            const char *hostname,
                            const char *servname) {
	uv_loop_t *loop = namereq->loop;
	uv_req_t *request = (uv_req_t *)namereq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, hostname);
			lua_pushstring(thread, servname);
			nret = 2;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumecoreq(loop, request, nret);
	}
}
static void uv_onservnamed (uv_getnameinfo_t *namereq,
                            int err,
                            const char *hostname,
                            const char *servname) {
	uv_loop_t *loop = namereq->loop;
	uv_req_t *request = (uv_req_t *)namereq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	(void)hostname;
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, servname);
			nret = 1;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumecoreq(loop, request, nret);
	}
}
static void uv_oncannonical (uv_getaddrinfo_t *addrreq,
                             int err,
                             struct addrinfo* results) {
	uv_loop_t *loop = addrreq->loop;
	uv_req_t *request = (uv_req_t *)addrreq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	if (thread) {
		int nret;
		if (!err) {
			lua_pushstring(thread, results->ai_canonname);
			nret = 1;
		}
		else nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumecoreq(loop, request, nret);
	}
	uv_freeaddrinfo(results);
}
static int k_setupnameaddr (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
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
		for (; *mode; mode++) switch (*mode) {
			case 'l': flags |= NI_NOFQDN; break;
			case 'd': flags |= NI_DGRAM; break;
#ifdef NI_IDN
			case 'i': flags |= NI_IDN; break;
#ifndef _GNU_SOURCE
			case 'u': flags |= NI_IDN|NI_IDN_ALLOW_UNASSIGNED; break;
			case 'a': flags |= NI_IDN|NI_IDN_USE_STD3_ASCII_RULES; break;
#endif
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
			struct sockaddr *addr = checknetaddr(L, 1);
			flags |= NI_NAMEREQD;
			err = uv_getnameinfo(loop, namereq, uv_onaddrnamed, addr, flags);
		}
	}
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_nameaddr (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetcoreqk(L, sched, k_setupnameaddr, NULL, NULL);
}


/*
 * Network Interface Information
 */

typedef struct NetInfoList {
	int count;
	uv_interface_address_t *list;
} NetInfoList;

#define checknetinfo(L)	((NetInfoList *)luaL_checkudata(L, 1, LCU_NETINFOLISTCLS))

static NetInfoList *openednetinfo (lua_State *L) {
	NetInfoList *netinfo = checknetinfo(L);
	luaL_argcheck(L, netinfo->list, 1, "closed "LCU_NETINFOLISTCLS);
	return netinfo;
}

/* getmetatable(cpuinfo).__{gc,close}(cpuinfo) */
static int netinfo_gc (lua_State *L) {
	NetInfoList *netinfo = checknetinfo(L);
	if (netinfo->list) {
		uv_free_interface_addresses(netinfo->list, netinfo->count);
		netinfo->list = NULL;
		netinfo->count = 0;
	}
	return 0;
}

/* cpucount = getmetatable(netinfo).__len(netinfo) */
static int netinfo_len (lua_State *L) {
	NetInfoList *netinfo = openednetinfo(L);
	lua_pushinteger(L, netinfo->count);
	return 1;
}

/* ... = getmetatable(cpuinfo).__call(cpuinfo, what [, previdx]) */
#define nibble2hex(N)	((N) < 10 ? '0'+(N) : 'a'+(N)%10);
static int netinfo_call (lua_State *L) {
	NetInfoList *netinfo = openednetinfo(L);
	const char *mode = luaL_checkstring(L, 2);
	int i = (int)luaL_optinteger(L, 3, 0);
	if (0 <= i && i < netinfo->count) {
		uv_interface_address_t *iface = netinfo->list+i;
		lua_settop(L, 2);
		lua_pushinteger(L, i+1);
		for (; *mode; mode++) switch (*mode) {
			case 'n': lua_pushstring(L, iface->name); break;
			case 'i': lua_pushboolean(L, iface->is_internal); break;
			case 'd': pushaddrtype(L, iface->address.address4.sin_family); break;
			case 'B': lua_pushlstring(L, iface->phys_addr, 6); break;
			case 'T': {
				char literal[17];
				int i = 0;
				while (1) {
					unsigned char byte = (unsigned char)iface->phys_addr[i/3];
					literal[i++] = nibble2hex(byte>>4);
					literal[i++] = nibble2hex(byte&0x0f);
					if (i == 17) break;
					literal[i++] = ':';
				}
				lua_pushlstring(L, literal, 17);
			} break;
			case 'b': {
				size_t sz;
				const char *s = getaddrbytes((struct sockaddr *)&iface->address, &sz);
				lua_pushlstring(L, s, sz);
			} break;
			case 't': {
				char s[LCU_ADDRMAXLITERAL];
				getaddrliteral((struct sockaddr *)&iface->address, s);
				lua_pushstring(L, s);
			} break;
			case 'l': {
				size_t sz;
				const unsigned char *mask = (const unsigned char *)
					getaddrbytes((struct sockaddr *)&iface->netmask, &sz);
				lua_Integer masklen;
				unsigned char byte;
				while (sz && !mask[--sz]);
				masklen = CHAR_BIT*(sz+1);
				for (byte = ~(mask[sz]); byte; byte >>= 1) masklen--;
				lua_pushinteger(L, masklen);
			} break;
			case 'm': {
				size_t sz;
				const char *s = getaddrbytes((struct sockaddr *)&iface->netmask, &sz);
				lua_pushlstring(L, s, sz);
			} break;
			default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
		}
		return lua_gettop(L)-2;
	}
	return 0;
}

typedef int (*GetStringByIdxFunc) (unsigned int idx, char *buffer, size_t *len);

/* netinfo = system.netinfo(option, which) */
static int system_netinfo (lua_State *L) {
	static const char *const options[] = { "id", "name", "all", NULL };
	int option = luaL_checkoption(L, 1, NULL, options);
	switch (option) {
		case 0:    /* id */
		case 1: {  /* name */
			lua_Integer i = luaL_checkinteger(L, 2);
			GetStringByIdxFunc f = option ? uv_if_indextoname : uv_if_indextoiid;
			char buffer[UV_IF_NAMESIZE];
			size_t len = sizeof(buffer);
			int err;
			luaL_argcheck(L, 0 < i && i <= INT_MAX, 2, "out of range");
			err = f((unsigned int)i, buffer, &len);
			if (err < 0) return lcuL_pusherrres(L, err);
			lua_pushlstring(L, buffer, len);
			return 1;
		}
		default: {  /* all */
			NetInfoList *netinfo = (NetInfoList *)lua_newuserdatauv(L, sizeof(NetInfoList), 0);
			int err = uv_interface_addresses(&netinfo->list, &netinfo->count);
			if (err < 0) return lcuL_pusherrres(L, err);
			luaL_setmetatable(L, LCU_NETINFOLISTCLS);
			lua_replace(L, 1);  /* cpuinfo replaces 'option' */
			lua_settop(L, 2);
			lua_pushinteger(L, 0);
			lua_pushvalue(L, -3);  /* netinfo */
			return 4;
		}
	}
}


/*
 * Socket
 */

#define FLAG_UDPCONNECTED 0x04


/* socket [, errmsg] = system.socket(type, domain) */
static int system_socket (lua_State *L) {
	static const char *const types[] = { "stream", "passive", "datagram", NULL };
	static const char *const domains[] = { "ipv4", "ipv6", "local", "share", NULL };
	lcu_Scheduler *sched = lcu_getsched(L);
	uv_loop_t *loop = lcu_toloop(sched);
	int type = luaL_checkoption(L, 1, NULL, types);
	int domain = luaL_checkoption(L, 2, NULL, domains);
	int err = UV_EAI_SOCKTYPE;
	if (domain < 2) {
		domain = AddrTypeId[domain];
		switch (type) {
			case 0:
			case 1: {
				const char *class = type ? LCU_TCPPASSIVECLS : LCU_TCPACTIVECLS;
				lcu_TcpSocket *tcp = lcuT_newudhdl(L, lcu_TcpSocket, class);
				err = uv_tcp_init_ex(loop, lcu_ud2hdl(tcp), domain);
				if (!err) tcp->flags = (domain == AF_INET6) ? LCU_SOCKIPV6FLAG : 0;
			} break;
			case 2: {
				lcu_UdpSocket *udp = lcuT_newudhdl(L, lcu_UdpSocket, LCU_UDPSOCKETCLS);
				err = uv_udp_init_ex(loop, lcu_ud2hdl(udp), domain);
				if (!err) udp->flags = (domain == AF_INET6) ? LCU_SOCKIPV6FLAG : 0;
			} break;
		}
	} else {
		switch (type) {
			case 0:
			case 1: {
				int socktranf = domain-2;
				const char *class = type == 1 ? LCU_PIPEPASSIVECLS :
				                    socktranf ? LCU_PIPESHARECLS : LCU_PIPEACTIVECLS;
				lcu_PipeSocket *pipe = lcuT_newudhdl(L, lcu_PipeSocket, class);
				err = uv_pipe_init(loop, lcu_ud2hdl(pipe), type ? 0 : socktranf);
				if (!err) pipe->flags = socktranf ? LCU_SOCKTRANFFLAG : 0;
			} break;
		}
	}
	return lcuL_pushresults(L, 1, err);
}

#define toclass(L) lua_tostring(L, lua_upvalueindex(1))


/* getmetatable(object).__gc(object) */
static int object_gc (lua_State *L) {
	luaL_checkudata(L, 1, toclass(L));
	lcu_closeudhdl(L, 1);
	return 0;
}


/* true = object:close() */
static int object_close (lua_State *L) {
	lcu_openedudhdl(L, 1, toclass(L));
	lcu_closeudhdl(L, 1);
	lua_pushboolean(L, 1);
	return 1;
}


/*
 * IP Sockets
 */

static const char *const AddrSites[] = {"self", "peer", NULL};

#define netdomainof(O)	(lcuL_maskflag(O, LCU_SOCKIPV6FLAG) ? AF_INET6 : AF_INET)

static struct sockaddr *settopaddrarg (lua_State *L, int arg, int domain, int *sz) {
	struct sockaddr *addr = tonetaddr(L, arg);
	if (addr) {
		lua_settop(L, arg);
		chkaddrdom(L, arg, addr, domain);
	} else {
		lua_settop(L, arg-1);
		addr = newaddress(L, domain);
	}
	*sz = (int)lua_rawlen(L, arg);
	return addr;
}

static const struct sockaddr *toobjaddr (lua_State *L, int idx, int domain) {
	struct sockaddr *address = checknetaddr(L, idx);
	chkaddrdom(L, idx, address, domain);
	return address;
}

/* domain = ipsock:getdomain() */
static int ipsock_getdomain (lua_State *L) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)luaL_checkudata(L, 1, toclass(L));
	pushaddrtype(L, netdomainof(udhdl));
	return 1;
}

static void completereqop (uv_loop_t *loop, uv_req_t *request, int err) {
	lua_State *thread = lcuU_endcoreq(loop, request);
	if (thread) {
		int nret = lcuL_pushresults(thread, 0, err);
		lcuU_resumecoreq(loop, request, nret);
	}
}

/*
 * UDP
 */

#define openedudp(L)	((lcu_UdpSocket *)lcu_openedudhdl(L,1,LCU_UDPSOCKETCLS))


/* address [, errmsg] = udp:getaddress([site [, address]]) */
static int udp_getaddress (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_ud2hdl(udp);
	int peer = luaL_checkoption(L, 2, "self", AddrSites);
	int domain = netdomainof(udp);
	int addrsz;
	struct sockaddr *addr = settopaddrarg(L, 3, domain, &addrsz);
	int err = peer ? uv_udp_getpeername(handle, addr, &addrsz)
	               : uv_udp_getsockname(handle, addr, &addrsz);
	lcu_assert((lua_Unsigned)addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = udp:bind(address) */
static int udp_bind (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	const struct sockaddr *addr = toobjaddr(L, 2, netdomainof(udp));
	int err = uv_udp_bind(lcu_ud2hdl(udp), addr, 0);
	return lcuL_pushresults(L, 0, err);
}



/* succ [, errmsg] = udp:setoption(name, value) */
static int udp_setoption (lua_State *L) {
	static const char * const options[] = { "mcastleave", "mcastjoin",
	                                        "mcastiface", "mcastttl",
	                                        "mcastloop", "broadcast", NULL };
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = lcu_ud2hdl(udp);
	int opt = luaL_checkoption(L, 2, NULL, options);
	int err;
	switch (opt) {
		case 0:    /* mcastleave */
		case 1: {  /* mcastjoin */
			uv_membership action = opt ? UV_JOIN_GROUP : UV_LEAVE_GROUP;
			const char *mcast = luaL_checkstring(L, 3);
			const char *iface = luaL_optstring(L, 4, NULL);
			const char *src = luaL_optstring(L, 5, NULL);
			if (!src) err = uv_udp_set_membership(handle, mcast, iface, action);
			else err = uv_udp_set_source_membership(handle, mcast, iface, src, action);
		}; break;
		case 2: {  /* mcastiface */
			const char *iface = luaL_optstring(L, 3, NULL);
			err = uv_udp_set_multicast_interface(handle, iface);
		}; break;
		case 3: {  /* mcastttl */
			lua_Integer value = luaL_checkinteger(L, 3);
			luaL_argcheck(L, 0 < value && value < 256, 3, "must be from 1 upto 255");
			err = uv_udp_set_multicast_ttl(handle, (int)value);
		}; break;
		case 4: {  /* mcastloop */
			int enabled = lua_toboolean(L, 3);
			err = uv_udp_set_multicast_loop(handle, enabled);
		}; break;
		case 5: {  /* broadcast */
			int enabled = lua_toboolean(L, 3);
			err = uv_udp_set_broadcast(handle, enabled);
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}


/* succ [, errmsg] = udp:connect([address]) */
static int udp_connect (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	const struct sockaddr *addr = lua_isnil(L, 2) ?
	                              NULL : toobjaddr(L, 2, netdomainof(udp));
	int err = uv_udp_connect(lcu_ud2hdl(udp), addr);
	if (err >= 0) {
		if (addr) lcuL_setflag(udp, FLAG_UDPCONNECTED);
		else lcuL_clearflag(udp, FLAG_UDPCONNECTED);;
	}
	return lcuL_pushresults(L, 0, err);
}



/* sent [, errmsg] = udp:write(data [, i [, j [, address]]]) */
static void uv_onsent (uv_udp_send_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupsend (lua_State *L,
                        uv_req_t *request,
                        uv_loop_t *loop,
                        lcu_Operation *op) {
	lcu_UdpSocket *udp = (lcu_UdpSocket *)lua_touserdata(L, 1);
	uv_udp_send_t *send = (uv_udp_send_t *)request;
	uv_udp_t *handle = (uv_udp_t *)lcu_ud2hdl(udp);
	uv_buf_t bufs[1];
	const struct sockaddr *addr = lcuL_maskflag(udp, FLAG_UDPCONNECTED) ?
	                              NULL : toobjaddr(L, 5, netdomainof(udp));
	int err;
	lcu_getinputbuf(L, 2, bufs);
	err = uv_udp_send(send, handle, bufs, 1, addr, uv_onsent);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int udp_write (lua_State *L) {
	lcu_UdpSocket *udp = openedudp(L);
	uv_udp_t *handle = (uv_udp_t *)lcu_ud2hdl(udp);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setupsend, NULL, NULL);
}


/* bytes [, errmsg] = udp:read(buffer [, i [, j [, address]]]) */
static int k_udprecv (lua_State *L) {
	lcu_UdpSocket *udp = (lcu_UdpSocket *)lua_touserdata(L, 1);
	if (lua_isinteger(L, 6)) {
		const struct sockaddr *src = (const struct sockaddr *)lua_touserdata(L, 8);
		struct sockaddr *dst = tonetaddr(L, 5);
		lua_pop(L, 1);  /* discard 'addr' lightuserdata */
		if (dst) {
			lcu_assert(src->sa_family == netdomainof(udp));
			memcpy(dst, src, src->sa_family == AF_INET ? sizeof(struct sockaddr_in)
			                                           : sizeof(struct sockaddr_in6));
		}
	}
	return lua_gettop(L)-5;
}
static int k_udpbuffer (lua_State *L) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_buf_t *buf = (uv_buf_t *)lua_touserdata( L, -1);
	lcu_assert(buf);
	lua_pop(L, 1);  /* discard 'buf' */
	lcu_getoutputbuf(L, 2, buf);
	if (buf->len > 65536) buf->len = 65536;  /* avoid use of recvmmsg by libuv */
	udhdl->step = k_udprecv;  /* update continuation function */
	return -1;  /* yield on success */
}
static void uv_onudprecv (uv_udp_t *udp,
                          ssize_t nread,
                          const uv_buf_t *buf,
                          const struct sockaddr *addr,
                          unsigned int flags) {
	uv_handle_t *handle = (uv_handle_t *)udp;
	if (nread == 0 && addr == NULL) {
		/* 'libuv' indication of datagram end (meaning?!), ignore it and */
		/* get ready to restart all over again from obtaining the buffer */
		lcu_UdataHandle *udhdl = lcu_hdl2ud(handle);
		udhdl->step = k_udpbuffer;
	} else {
		lua_State *thread = (lua_State *)handle->data;
		int nret;
		lcu_assert(thread);
		lcu_assert(buf->base != (char *)buf);
		lcu_assert(addr);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			lua_pushboolean(thread, flags&UV_UDP_PARTIAL);
			lua_pushlightuserdata(thread, (void *)addr);
			nret = 3;
		}
		else nret = lcuL_pusherrres(thread, nread);
		lcuU_resumeudhdl(handle, nret);
	}
}
static void uv_ongetbuffer (uv_handle_t *handle,
                            size_t suggested_size,
                            uv_buf_t *buf) {
	(void)suggested_size;
	do {
		lua_State *thread = (lua_State *)handle->data;
		buf->base = (char *)buf;
		buf->len = 0;
		lcu_assert(thread);
		lua_pushlightuserdata(thread, buf);
		if (lua_status(thread) == LUA_OK) {
			/* this might happen because libuv might call 'uv_alloc_cb' inside */
			/* 'uv_*_start' before it returns successfully. In such case, we */
			/* are still running inside the calling coroutine. */
			/* Ref.: https://groups.google.com/g/libuv/c/bTwH1X_F4p4 */
			lcu_UdataHandle *udhdl = lcu_hdl2ud(handle);
			int nret = udhdl->step(thread);
			lcu_assert(nret == -1);  /* must yield, we can't return from here */
		} else {
			lcu_assert(lua_status(thread) == LUA_YIELD);
			lcuU_resumeudhdl(handle, 1);
		}
	} /* while 'socket:read' is called again after error getting buffer */
	while (handle->data && buf->base == (char *)buf);
}
static int udpstoprecv (uv_handle_t *handle) {
	lcu_log(handle, handle->data, "uv_udp_recv_stop");
	return uv_udp_recv_stop((uv_udp_t *)handle);
}
static int udpstartrecv (uv_handle_t *handle) {
	lcu_log(handle, handle->data, "uv_udp_recv_start");
	return uv_udp_recv_start((uv_udp_t *)handle, uv_ongetbuffer, uv_onudprecv);
}
static int udp_read (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, LCU_UDPSOCKETCLS);
	lua_settop(L, 5);
	return lcuT_resetudhdlk(L, udhdl, udpstartrecv, udpstoprecv, k_udpbuffer);
}

/*
 * Stream
 */

/* succ [, errmsg] = stream:shutdown() */
static void uv_onshutdown (uv_shutdown_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupshutdown (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_stream_t *stream = (uv_stream_t *)lcu_ud2hdl(udhdl);
	uv_shutdown_t *shutdown = (uv_shutdown_t *)request;
	int err = uv_shutdown(shutdown, stream, uv_onshutdown);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int active_shutdown (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, toclass(L));
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setupshutdown, NULL, NULL);
}


/* sent [, errmsg] = stream:write(data [, i [, j]]) */
static void uv_onwritten (uv_write_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setupwrite (lua_State *L,
                         uv_req_t *request,
                         uv_loop_t *loop,
                         lcu_Operation *op) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_stream_t *stream = (uv_stream_t *)lcu_ud2hdl(udhdl);
	uv_write_t *write = (uv_write_t *)request;
	uv_buf_t bufs[1];
	int err;
	lcu_getinputbuf(L, 2, bufs);
	err = uv_write(write, stream, bufs, 1, uv_onwritten);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int active_write (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, toclass(L));
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setupwrite, NULL, NULL);
}


/* sent [, errmsg] = pipe:write(data [, i [, j [, object]]]) */
static int k_setupwriteobj (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_stream_t *stream = (uv_stream_t *)lcu_ud2hdl(udhdl);
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
			                                       LCU_PIPESHARECLS,
			                                       NULL };
			int i;
			for (i = 0; classes[i]; i++) {
				int samemt = 0;
				luaL_getmetatable(L, classes[i]);  /* get correct metatable */
				samemt = lua_rawequal(L, -1, -2);
				lua_pop(L, 1);  /* remove object metatables */
				if (samemt) {
					lcu_UdataHandle *wrtobj = (lcu_UdataHandle *)lua_touserdata(L, 5);
					wrtstrm = (uv_stream_t *)lcu_ud2hdl(wrtobj);
					break;
				}
			}
			lua_pop(L, 1);  /* remove expected metatables */
		}
		luaL_argcheck(L, wrtstrm, 5, "writable object expected");
	}
	lcu_getinputbuf(L, 2, bufs);
	err = uv_write2(write, stream, bufs, 1, wrtstrm, uv_onwritten);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int pipe_write (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, LCU_PIPESHARECLS);
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setupwriteobj, NULL, NULL);
}

/* bytes [, errmsg] = stream:read(buffer [, i [, j]]) */
static int k_recvdata (lua_State *L) {
	return lua_gettop(L)-5;
}
static int k_getbuffer (lua_State *L) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	lua_CFunction nextstep = (lua_CFunction)lua_touserdata(L, 5);
	uv_buf_t *buf = (uv_buf_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);  /* discard 'buf' pointer */
	lcu_assert(nextstep);
	lcu_assert(buf);
	lcu_getoutputbuf(L, 2, buf);
	udhdl->step = nextstep;
	return -1;
}
static void uv_onrecvdata (uv_stream_t *stream,
                           ssize_t nread,
                           const uv_buf_t *buf) {
	uv_handle_t *handle = (uv_handle_t *)stream;
	lua_State *thread = (lua_State *)handle->data;
	lcu_UdataHandle *udhdl = lcu_hdl2ud(handle);
	if (nread == 0) {
		/* 'libuv' indication of EAGAIN or EWOULDBLOCK, ignore it and */
		/* get ready to restart all over again from obtaining the buffer */
		udhdl->step = k_getbuffer;
	} else {
		int nret;
		lcu_assert(thread);
		lcu_assert(buf->base != (char *)buf);
		if (nread >= 0) {
			lua_pushinteger(thread, nread);
			nret = 1;
		} else {
			/* in case of errors, 'libuv' might not call 'uv_ongetbuffer' */
			/* so 'udhdl->step==k_getbuffer' making the resume below fail. */
			udhdl->step = k_recvdata;  /* also avoid calling 'k_recvpipedata' on error */
			if (nread == UV_EOF) udhdl->stop = NULL;  /* implies 'handle' is already stopped */
			nret = lcuL_pusherrres(thread, nread);
		}
		lcuU_resumeudhdl(handle, nret);
	}
}
static int stoprecvdata (uv_handle_t *handle) {
	lcu_log(handle, handle->data, "uv_read_stop");
	return uv_read_stop((uv_stream_t *)handle);
}
static int startrecvdata (uv_handle_t *handle) {
	lcu_log(handle, handle->data, "uv_read_start");
	return uv_read_start((uv_stream_t *)handle, uv_ongetbuffer, uv_onrecvdata);
}
static int active_read (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, toclass(L));
	lua_settop(L, 4);
	lua_pushlightuserdata(L, k_recvdata);
	return lcuT_resetudhdlk(L, udhdl, startrecvdata, stoprecvdata, k_getbuffer);
}


/* bytes [, errmsg] = pipe:read(buffer [, i [, j]]) */
static int pushstreamread (lua_State *L, uv_pipe_t *pipe) {
	uv_handle_type type = uv_pipe_pending_type(pipe);
	lcu_UdataHandle *udhdl;
	uv_stream_t *stream;
	int err;
	switch (type) {
		case UV_NAMED_PIPE: {
			lcu_PipeSocket *newpipe = lcuT_newudhdl(L, lcu_PipeSocket, LCU_PIPEACTIVECLS);
			uv_pipe_t *handle = lcu_ud2hdl(newpipe);
			err = uv_pipe_init(pipe->loop, handle, 0);
			udhdl = (lcu_UdataHandle*)newpipe;
			stream = (uv_stream_t *)handle;
		} break;
		case UV_TCP: {
			lcu_TcpSocket *tcp = lcuT_newudhdl(L, lcu_TcpSocket, LCU_TCPACTIVECLS);
			uv_tcp_t *handle = lcu_ud2hdl(tcp);
			err = uv_tcp_init(pipe->loop, handle);
			udhdl = (lcu_UdataHandle*)tcp;
			stream = (uv_stream_t *)handle;
		} break;
		default: return UV_EAI_SOCKTYPE;
	}
	if (err) return err;
	lcuL_clearflag(udhdl, LCU_HANDLECLOSEDFLAG);
	return uv_accept((uv_stream_t *)pipe, stream);
}
static int k_recvpipedata (lua_State *L) {
	lcu_UdataHandle *udhdl = lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	uv_pipe_t *pipe = (uv_pipe_t *)handle;
	if (uv_pipe_pending_count(pipe)) {  /* only if read was successful? */
		int err = pushstreamread(L, pipe);
		return lcuL_pushresults(L, lua_gettop(L)-5, err);
	}
	return lua_gettop(L)-5;
}
static int pipe_read (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, LCU_PIPESHARECLS);
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	uv_pipe_t *pipe = (uv_pipe_t *)handle;
	if (uv_pipe_pending_count(pipe)) {
		int err;
		lua_pushinteger(L, 0);  /* byte count should be zero */
		err = pushstreamread(L, pipe);
		return lcuL_pushresults(L, 2, err);
	}
	lua_settop(L, 4);
	lua_pushlightuserdata(L, k_recvpipedata);
	return lcuT_resetudhdlk(L, udhdl, startrecvdata, stoprecvdata, k_getbuffer);
}


#define LISTENCONN_UNIT 0x04

#define islisteningconn(O)	((O)->flags >= LISTENCONN_UNIT)

#define addlistenedconn(O)	((O)->flags += LISTENCONN_UNIT)

static int picklistenedconn (lcu_UdataHandle *udhdl) {
	if (udhdl->flags < 2*LISTENCONN_UNIT) return 0;
	udhdl->flags -= LISTENCONN_UNIT;
	return 1;
}


/* succ [, errmsg] = passive:listen(backlog) */
static void uv_onconnection (uv_stream_t *stream, int status) {
	uv_handle_t *handle = (uv_handle_t *)stream;
	lua_State *thread = (lua_State *)handle->data;
	if (thread) {
		lua_pushinteger(thread, status);
		lcuU_resumeudhdl(handle, 1);
		if (handle->data == NULL) uv_unref(handle);
		else lcu_assert(uv_has_ref(handle));
	}
	else addlistenedconn(lcu_hdl2ud(handle));
	lcuU_checksuspend(handle->loop);
}
static int passive_listen (lua_State *L) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, toclass(L));
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	lua_Integer backlog = luaL_checkinteger(L, 2);
	int err;
	luaL_argcheck(L, !islisteningconn(udhdl), 1, "already listening");
	luaL_argcheck(L, 0 <= backlog && backlog <= INT_MAX, 2, "out of range");
	err = uv_listen((uv_stream_t *)handle, (int)backlog, uv_onconnection);
	if (err >= 0) {
		addlistenedconn(udhdl);  /* mark socket as listening */
		uv_unref(handle);  /* emulate a 'uv_listen_stop(uv_stream_t *)' */
	}
	return lcuL_pushresults(L, 0, err);
}


typedef int (*NewAcceptFunc) (lua_State *L,
                              uv_loop_t *loop,
                              lcu_UdataHandle *udhdl,
                              lcu_UdataHandle **newudhdl);

/* stream [, errmsg] = passive:accept() */
static int k_acceptstream (lua_State *L) {
	lcu_UdataHandle *udhdl = (lcu_UdataHandle *)lua_touserdata(L, 1);
	uv_handle_t *handle = lcu_ud2hdl(udhdl);
	NewAcceptFunc newaccept = (NewAcceptFunc)lua_touserdata(L, 2);
	lcu_UdataHandle *newudhdl;
	int err = (int)lua_tointeger(L, 3);
	if (err >= 0) {
		err = newaccept(L, handle->loop, udhdl, &newudhdl);
		if (err >= 0) {
			err = uv_accept((uv_stream_t *)handle,
			                (uv_stream_t *)lcu_ud2hdl(newudhdl));
			if (err >= 0) lcuL_clearflag(newudhdl, LCU_HANDLECLOSEDFLAG);
		}
		else addlistenedconn(udhdl);
	}
	return lcuL_pushresults(L, 1, err);
}
static int stoplisten (uv_handle_t *handle) {
	uv_unref(handle);  /* emulate a 'uv_listen_stop(uv_stream_t *)' */
	return 0;
}
static int startlisten (uv_handle_t *handle) {
	uv_ref(handle);  /* emulate a 'uv_listen_start(uv_stream_t *)' */
	return 0;
}
static int listen_accept (lua_State *L,
                          const char *class,
                          NewAcceptFunc newaccept) {
	lcu_UdataHandle *udhdl = lcu_openedudhdl(L, 1, class);
	luaL_argcheck(L, islisteningconn(udhdl), 1, "not listening");
	lua_settop(L, 1);
	lua_pushlightuserdata(L, newaccept);
	if (picklistenedconn(udhdl)) return k_acceptstream(L);
	return lcuT_resetudhdlk(L, udhdl, startlisten, stoplisten, k_acceptstream);
}


/*
 * TCP
 */

#define openedtcp(L,C)	((lcu_TcpSocket *)lcu_openedudhdl(L,1,C))


/* address [, errmsg] = tcp:getaddress([site [, address]]) */
static int tcp_getaddress (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, toclass(L));
	uv_tcp_t *handle = lcu_ud2hdl(tcp);
	int peer = luaL_checkoption(L, 2, "self", AddrSites);
	int domain = netdomainof(tcp);
	int addrsz;
	struct sockaddr *addr = settopaddrarg(L, 3, domain, &addrsz);
	int err = peer ? uv_tcp_getpeername(handle, addr, &addrsz)
	               : uv_tcp_getsockname(handle, addr, &addrsz);
	lcu_assert((lua_Unsigned)addrsz == lua_rawlen(L, 3));
	return lcuL_pushresults(L, 1, err);
}


/* succ [, errmsg] = tcp:bind(address) */
static int tcp_bind (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, toclass(L));
	const struct sockaddr *addr = toobjaddr(L, 2, netdomainof(tcp));
	int err = uv_tcp_bind(lcu_ud2hdl(tcp), addr, 0);
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
			err = uv_tcp_keepalive(lcu_ud2hdl(tcp), enabled, delay);
		}; break;
		case 1: {  /* nodelay */
			err = uv_tcp_nodelay(lcu_ud2hdl(tcp), enabled);
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}


/* succ [, errmsg] = tcp:connect(address) */
static void uv_onconnected (uv_connect_t *request, int err) {
	completereqop(request->handle->loop, (uv_req_t *)request, err);
}
static int k_setuptcpconn (lua_State *L,
                           uv_req_t *request,
                           uv_loop_t *loop,
                           lcu_Operation *op) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)lua_touserdata(L, 1);
	const struct sockaddr *addr = toobjaddr(L, 2, netdomainof(tcp));
	uv_connect_t *connect = (uv_connect_t *)request;
	int err = uv_tcp_connect(connect, lcu_ud2hdl(tcp), addr, uv_onconnected);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int tcp_connect (lua_State *L) {
	lcu_TcpSocket *tcp = openedtcp(L, LCU_TCPACTIVECLS);
	uv_tcp_t *handle = lcu_ud2hdl(tcp);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setuptcpconn, NULL, NULL);
}


/* tcp [, errmsg] = tcp:accept() */
static int newtcpaccept (lua_State *L,
                         uv_loop_t *loop,
                         lcu_UdataHandle *udhdl,
                         lcu_UdataHandle **newudhdl) {
	lcu_TcpSocket *tcp = lcuT_newudhdl(L, lcu_TcpSocket, LCU_TCPACTIVECLS);
	int err = uv_tcp_init(loop, lcu_ud2hdl(tcp));
	if (!err) {
		tcp->flags |= lcuL_maskflag(udhdl, LCU_SOCKIPV6FLAG);
		*newudhdl = (lcu_UdataHandle *)tcp;
	}
	return err;
}
static int tcp_accept (lua_State *L) {
	return listen_accept(L, LCU_TCPPASSIVECLS, newtcpaccept);
}


/*
 * Pipe
 */

#define openedpipe(L,c)	((lcu_PipeSocket *)lcu_openedudhdl(L, 1, c))


/* domain = pipe:getdomain() */
static int pipe_getdomain (lua_State *L) {
	lcu_PipeSocket *pipe = openedpipe(L, toclass(L));
	lua_pushstring(L, lcuL_maskflag(pipe, LCU_SOCKTRANFFLAG) ? "share" : "local");
	return 1;
}

/* address [, errmsg] = pipe:getaddress([site]) */
static int pipe_getaddress (lua_State *L) {
	lcu_PipeSocket *pipe = openedpipe(L, toclass(L));
	uv_pipe_t *handle = lcu_ud2hdl(pipe);
	int peer = luaL_checkoption(L, 2, "self", AddrSites);
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
	lcu_PipeSocket *pipe = openedpipe(L, toclass(L));
	const char *addr = luaL_checkstring(L, 2);
	int err = uv_pipe_bind(lcu_ud2hdl(pipe), addr);
	return lcuL_pushresults(L, 0, err);
}


static const char * const PipeOptions[] = {"permission", NULL};

/* succ [, errmsg] = pipe:setoption(name, value) */
static int pipe_setoption (lua_State *L) {
	lcu_PipeSocket *pipe = openedpipe(L, toclass(L));
	uv_pipe_t *handle = lcu_ud2hdl(pipe);
	int opt = luaL_checkoption(L, 2, NULL, PipeOptions);
	int err;
	switch (opt) {
		case 0: {  /* permission */
			const char *mode = luaL_optstring(L, 3, "");
			int flags = 0;
			for (; *mode; mode++) switch (*mode) {
				case 'r': flags |= UV_READABLE; break;
				case 'w': flags |= UV_WRITABLE; break;
				default: return luaL_error(L, "unknown option (got '%c')", *mode);
			}
			err = uv_pipe_chmod(handle, flags);
		}; break;
		default: return 0;
	}
	return lcuL_pushresults(L, 0, err);
}


/* succ [, errmsg] = pipe:connect(address) */
static int k_setuppipeconn (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	lcu_PipeSocket *pipe = (lcu_PipeSocket *)lua_touserdata(L, 1);
	const char *addr = luaL_checkstring(L, 2);
	uv_connect_t *connect = (uv_connect_t *)request;
	uv_pipe_connect(connect, lcu_ud2hdl(pipe), addr, uv_onconnected);
	lcuT_armcoreq(L, loop, op, 0);
	return -1;  /* yield on success */
}
static int pipe_connect (lua_State *L) {
	lcu_PipeSocket *pipe = openedpipe(L, toclass(L));
	uv_pipe_t *handle = lcu_ud2hdl(pipe);
	lcu_Scheduler *sched = lcu_tosched(handle->loop);
	return lcuT_resetcoreqk(L, sched, k_setuppipeconn, NULL, NULL);
}


static int newpipeaccept (lua_State *L,
                          uv_loop_t *loop,
                          lcu_UdataHandle *udhdl,
                          lcu_UdataHandle **newudhdl) {
	int socktranf = lcuL_maskflag(udhdl, LCU_SOCKTRANFFLAG);
	const char *class = socktranf ? LCU_PIPESHARECLS : LCU_PIPEACTIVECLS;
	lcu_PipeSocket *pipe = lcuT_newudhdl(L, lcu_PipeSocket, class);
	int err = uv_pipe_init(loop, lcu_ud2hdl(pipe), socktranf);
	if (!err) {
		pipe->flags |= socktranf;
		*newudhdl = (lcu_UdataHandle *)pipe;
	}
	return err;
}
static int pipe_accept (lua_State *L) {
	return listen_accept(L, LCU_PIPEPASSIVECLS, newpipeaccept);
}


/*
 * Terminal
 */

#define openedterm(L)	((lcu_TermSocket *)lcu_openedudhdl(L, 1, LCU_TERMSOCKETCLS))


/* success = terminal:setmode(mode) */
static int terminal_setmode (lua_State *L) {
	static const char *const modes[] = { "normal", "raw", "binary" };
	lcu_TermSocket *term = openedterm(L);
	uv_tty_t *tty = (uv_tty_t *)lcu_ud2hdl(term);
	int mode = luaL_checkoption(L, 2, "default", modes);
	int err = uv_tty_set_mode(tty, mode);
	return lcuL_pushresults(L, 0, err);
}

/* width, height = terminal:winsize() */
static int terminal_winsize (lua_State *L) {
	lcu_TermSocket *term = openedterm(L);
	uv_tty_t *tty = (uv_tty_t *)lcu_ud2hdl(term);
	int width, height;
	int err = uv_tty_get_winsize(tty, &width, &height);
	if (err) return lcuL_pusherrres(L, err);
	lua_pushinteger(L, width);
	lua_pushinteger(L, height);
	return 2;
}


/*
 * Module
 */

static const luaL_Reg addrmt[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg foundmt[] = {
	{"__gc", found_gc},
	{"__close", found_gc},
	{NULL, NULL}
};

static const luaL_Reg found[] = {
	{"close", found_close},
	{"next", found_next},
	{"reset", found_reset},
	{"getaddress", found_getaddress},
	{"getdomain", found_getdomain},
	{"getsocktype", found_getsocktype},
	{NULL, NULL}
};

static const luaL_Reg netinfomt[] = {
	{"__gc", netinfo_gc},
	{"__close", netinfo_gc},
	{"__len", netinfo_len},
	{"__call", netinfo_call},
	{NULL, NULL}
};

static const luaL_Reg objectmt[] = {
	{"__gc", object_gc},
	{"__close", object_gc},
	{NULL, NULL}
};

static const luaL_Reg object[] = {
	{"close", object_close},
	{NULL, NULL}
};

static const luaL_Reg ip[] = {
	{"getdomain", ipsock_getdomain},
	{NULL, NULL}
};

static const luaL_Reg udpsocket[] = {
	{"getaddress", udp_getaddress},
	{"bind", udp_bind},
	{"setoption", udp_setoption},
	{"connect", udp_connect},
	{"write", udp_write},
	{"read", udp_read},
	{NULL, NULL}
};

static const luaL_Reg active[] = {
	{"shutdown", active_shutdown},
	{"write", active_write},
	{"read", active_read},
	{NULL, NULL}
};

static const luaL_Reg passive[] = {
	{"listen", passive_listen},
	{NULL, NULL}
};

static const luaL_Reg tcpsocket[] = {
	{"getaddress", tcp_getaddress},
	{"bind", tcp_bind},
	{NULL, NULL}
};

static const luaL_Reg tcpactive[] = {
	{"setoption", tcp_setoption},
	{"connect", tcp_connect},
	{NULL, NULL}
};

static const luaL_Reg tcppassive[] = {
	{"accept", tcp_accept},
	{NULL, NULL}
};

static const luaL_Reg pipesocket[] = {
	{"getdomain", pipe_getdomain},
	{"getaddress", pipe_getaddress},
	{"bind", pipe_bind},
	{"setoption", pipe_setoption},
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
	{"write", pipe_write},
	{"read", pipe_read},
	{NULL, NULL}
};

static const luaL_Reg terminal[] = {
	{"setmode", terminal_setmode},
	{"winsize", terminal_winsize},
	{NULL, NULL}
};

static const luaL_Reg modulef[] = {
	{"address", system_address},
	{"netinfo", system_netinfo},
};

static const luaL_Reg upvaluef[] = {
	{"findaddr", system_findaddr},
	{"nameaddr", system_nameaddr},
	{"socket", system_socket},
	{NULL, NULL}
};

LCUI_FUNC void lcuM_addcommunf (lua_State *L) {
	luaL_newmetatable(L, LCU_NETADDRCLS);
	luaL_setfuncs(L, addrmt, 0);
	lua_pop(L, 1);

	luaL_newmetatable(L, LCU_NETADDRLISTCLS);
	luaL_setfuncs(L, foundmt, 0);
	lua_newtable(L);  /* create method table */
	luaL_setfuncs(L, found, 0);
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lua_pop(L, 1);  /* pop metatable */

	luaL_newmetatable(L, LCU_NETINFOLISTCLS);
	luaL_setfuncs(L, netinfomt, 0);
	lua_pop(L, 1);  /* pop metatable */

	lua_pushstring(L, LCU_UDPSOCKETCLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, ip, 1);
	luaL_setfuncs(L, udpsocket, 0);
	luaL_newmetatable(L, LCU_UDPSOCKETCLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_TCPACTIVECLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, ip, 1);
	lcuM_setfuncs(L, tcpsocket, 1);
	lcuM_setfuncs(L, active, 1);
	luaL_setfuncs(L, tcpactive, 0);
	luaL_newmetatable(L, LCU_TCPACTIVECLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_TCPPASSIVECLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, ip, 1);
	lcuM_setfuncs(L, tcpsocket, 1);
	lcuM_setfuncs(L, passive, 1);
	luaL_setfuncs(L, tcppassive, 0);
	luaL_newmetatable(L, LCU_TCPPASSIVECLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_PIPESHARECLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, pipesocket, 1);
	lcuM_setfuncs(L, active, 1);
	lcuM_setfuncs(L, pipeactive, 1);
	luaL_setfuncs(L, pipeipc, 0);
	luaL_newmetatable(L, LCU_PIPESHARECLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_PIPEACTIVECLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, pipesocket, 1);
	lcuM_setfuncs(L, active, 1);
	lcuM_setfuncs(L, pipeactive, 1);
	luaL_newmetatable(L, LCU_PIPEACTIVECLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_PIPEPASSIVECLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, pipesocket, 1);
	lcuM_setfuncs(L, passive, 1);
	luaL_setfuncs(L, pipepassive, 0);
	luaL_newmetatable(L, LCU_PIPEPASSIVECLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	lua_pushstring(L, LCU_TERMSOCKETCLS);
	lua_newtable(L);  /* create method table */
	lcuM_setfuncs(L, object, 1);
	lcuM_setfuncs(L, active, 1);
	luaL_setfuncs(L, terminal, 0);
	luaL_newmetatable(L, LCU_TERMSOCKETCLS);
	lua_insert(L, -2);  /* place below method table */
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lcuM_setfuncs(L, objectmt, 1);
	lua_pop(L, 2);  /* pop metatable and name */

	luaL_setfuncs(L, modulef, 0);
	lcuM_setfuncs(L, upvaluef, LCU_MODUPVS);
}
