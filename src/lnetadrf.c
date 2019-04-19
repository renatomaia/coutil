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

static void pushaddrtype (lua_State *L, int type) {
	switch (type) {
		case AF_INET: lua_pushliteral(L, "ipv4"); break;
		case AF_INET6: lua_pushliteral(L, "ipv6"); break;
		default: lua_pushliteral(L, "unsupported");
	}
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

#define addrbinsz(T) (T == AF_INET ? LCU_ADDRBINSZ_IPV4 : LCU_ADDRBINSZ_IPV6)

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


static const luaL_Reg addr[] = {
	{"__tostring", lcuM_addr_tostring},
	{"__eq", lcuM_addr_eq},
	{"__index", lcuM_addr_index},
	{"__newindex", lcuM_addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg modf[] = {
	{"address", lcuM_address},
	{NULL, NULL}
};

LCULIB_API void lcuM_addnetadrf (lua_State *L) {
	lcuM_setfuncs(L, modf, LCU_MODUPVS);
	loopL_newclass(L, LCU_NETADDRCLS, NULL);
	luaL_setfuncs(L, addr, 0);
	lua_pop(L, 1);
}
