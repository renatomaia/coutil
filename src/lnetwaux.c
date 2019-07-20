#include "lnetwaux.h"
#include "loperaux.h"
#include "looplib.h"

#include <string.h>


#define hdl2obj(H,T) (lcu_Object *)((char *)H - offsetof(lcu_Object, handle))

/*
 * Addresses
 */

LCULIB_API struct sockaddr *lcu_newaddress (lua_State *L, int type) {
	struct sockaddr *na;
	size_t sz;
	switch (type) {
		case AF_INET: sz = sizeof(struct sockaddr_in); break;
		case AF_INET6: sz = sizeof(struct sockaddr_in6); break;
		default: luaL_error(L, "invalid address type"); return NULL;
	}
	na = (struct sockaddr *)lua_newuserdata(L, sz);
	memset(na, 0, sz);
	luaL_setmetatable(L, LCU_NETADDRCLS);
	return na;
}


/*
 * Names
 */

typedef struct lcu_AddressList {
	struct addrinfo *start;
	struct addrinfo *current;
} lcu_AddressList;

LCULIB_API lcu_AddressList *lcu_newaddrlist (lua_State *L) {
	lcu_AddressList *l = (lcu_AddressList *)lua_newuserdata(L, sizeof(lcu_AddressList));
	lcu_setaddrlist(l, NULL);
	luaL_setmetatable(L, LCU_NETADDRLISTCLS);
	return l;
}

static void findvalid (lcu_AddressList *l) {
	for (; l->current; l->current = l->current->ai_next) {
		switch (l->current->ai_socktype) {
			case SOCK_DGRAM:
			case SOCK_STREAM: return;
		}
	}
}

LCULIB_API void lcu_setaddrlist (lcu_AddressList *l, struct addrinfo *addrs) {
	l->start = addrs;
	l->current = addrs;
	findvalid(l);
}

LCULIB_API struct addrinfo *lcu_getaddrlist (lcu_AddressList *l) {
	return l->start;
}

LCULIB_API struct addrinfo *lcu_peekaddrlist (lcu_AddressList *l) {
	return l->current;
}

LCULIB_API struct addrinfo *lcu_nextaddrlist (lcu_AddressList *l) {
	l->current = l->current->ai_next;
	findvalid(l);
	return l->current;
}


/*
 * Sockets
 */

#define FLAG_CLOSED 0x01
#define FLAG_OBJOPON 0x02
#define FLAG_IPV6DOM 0x04

#define FLAG_LISTEN 0x08  /* only for listen sockets */

#define FLAG_NODELAY 0x08  /* only for stream sockets */
#define FLAG_KEEPALIVE 0x10  /* only for stream sockets */

#define FLAG_CONNECTED 0x08  /* only for datagram sockets */
#define FLAG_BROADCAST 0x10  /* only for datagram sockets */
#define FLAG_MCASTLOOP 0x20  /* only for datagram sockets */
#define FLAG_MCASTTTL 0x3fd0  /* only for datagram sockets */

#define FLAG_LSTNMASK 0x0f  /* only for listen sockets */
#define FLAG_STRMMASK 0x1f  /* only for stream sockets */
#define FLAG_DGRMMASK 0x3f  /* only for datagram sockets */

#define LISTENCONN_UNIT (FLAG_LSTNMASK+1)
#define KEEPALIVE_SHIFT 4
#define MCASTTTL_SHIFT 6

struct lcu_Object {
	int flags;
	uv_handle_t handle;
};

struct lcu_UdpSocket {
	int flags;
	uv_udp_t handle;
	union {
		struct in_addr ipv4;
		struct in6_addr ipv6;
	} mcastiface;
};

struct lcu_TcpSocket {
	int flags;
	uv_tcp_t handle;
};

struct lcu_IpcPipe {
	int flags;
	uv_pipe_t handle;
};

static lcu_Object *createobj (lua_State *L, size_t size, const char *cls) {
	lcu_Object *object = (lcu_Object *)lua_newuserdata(L, size);
	object->handle.data = NULL;
	object->flags = FLAG_CLOSED;
	luaL_setmetatable(L, cls);
	return object;
}

LCULIB_API int lcu_closeobj (lua_State *L, int idx, const char *cls) {
	lcu_Object *object = (lcu_Object *)loopL_checkinstance(L, idx, cls);
	if (object && !lcuL_maskflag(object, FLAG_CLOSED)) {
		lcu_closeobjhdl(L, idx, &object->handle);
		lcuL_setflag(object, FLAG_CLOSED);
		return 1;
	}
	return 0;
}

LCULIB_API void lcu_enableobj (lcu_Object *object) {
	lcu_assert(lcuL_maskflag(object, FLAG_CLOSED));
	lcu_assert(object->handle.data == NULL);
	lcuL_clearflag(object, FLAG_CLOSED);
}

LCULIB_API int lcu_isobjclosed (lcu_Object *object) {
	return lcuL_maskflag(object, FLAG_CLOSED);
}

LCULIB_API uv_handle_t *lcu_toobjhdl (lcu_Object *object) {
	return &object->handle;
}

LCULIB_API lcu_Object *lcu_tohdlobj (uv_handle_t *handle) {
	const char *ptr = (const char *)handle;
	return (lcu_Object *)(ptr - offsetof(lcu_Object, handle));
}

LCULIB_API int lcu_getobjdomain (lcu_Object *obj) {
	return lcuL_maskflag(obj, FLAG_IPV6DOM) ? AF_INET6 : AF_INET;
}

LCULIB_API void lcu_addobjlisten (lcu_Object *object) {
	object->flags += LISTENCONN_UNIT;
}

LCULIB_API int lcu_pickobjlisten (lcu_Object *object) {
	if (object->flags < 2*LISTENCONN_UNIT) return 0;
	object->flags -= LISTENCONN_UNIT;
	return 1;
}

LCULIB_API int lcu_isobjlisten (lcu_Object *object) {
	return object->flags >= LISTENCONN_UNIT;
}

LCULIB_API void lcu_markobjlisten (lcu_Object *object) {
	lcu_assert(!lcu_isobjlisten(object));
	lcu_addobjlisten(object);
}


LCULIB_API lcu_UdpSocket *lcu_newudp (lua_State *L, int domain) {
	lcu_UdpSocket *udp = (lcu_UdpSocket *)createobj(L, sizeof(lcu_UdpSocket),
	                                                   LCU_UDPSOCKCLS);
	if (domain == AF_INET6) lcuL_setflag(udp, FLAG_IPV6DOM);
	lcuL_setflag(udp, 1<<MCASTTTL_SHIFT);
	return udp;
}

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int class, int domain) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)createobj(L, sizeof(lcu_TcpSocket),
	                                                   lcu_TcpSockCls[class]);
	if (domain == AF_INET6) lcuL_setflag(tcp, FLAG_IPV6DOM);
	return tcp;
}

LCULIB_API lcu_IpcPipe *lcu_newpipe (lua_State *L, int class) {
	return (lcu_IpcPipe *)createobj(L, sizeof(lcu_IpcPipe), lcu_IpcPipeCls[class]);
}

#define newbooloption(name, type, flag) \
	LCULIB_API int lcu_get##name(type *obj) { \
		return lcuL_maskflag(obj, flag); \
	} \
	\
	LCULIB_API void lcu_set##name(type *obj, int enabled) { \
		if (enabled) lcuL_setflag(obj, flag); \
		else lcuL_clearflag(obj, flag); \
	}

newbooloption(objarmed, lcu_Object, FLAG_OBJOPON);
newbooloption(udpconnected, lcu_UdpSocket, FLAG_CONNECTED);
newbooloption(udpbroadcast, lcu_UdpSocket, FLAG_BROADCAST);
newbooloption(udpmcastloop, lcu_UdpSocket, FLAG_MCASTLOOP);
newbooloption(tcpnodelay, lcu_TcpSocket, FLAG_NODELAY);

LCULIB_API int lcu_getudpmcastttl (lcu_UdpSocket *udp) {
	int value = lcuL_maskflag(udp, FLAG_MCASTTTL);
	if (value) return udp->flags>>MCASTTTL_SHIFT;
	return 0;
}

LCULIB_API void lcu_setudpmcastttl (lcu_UdpSocket *udp, int value) {
	if (value < 1) lcuL_clearflag(udp, FLAG_MCASTTTL);
	else udp->flags = (value<<MCASTTTL_SHIFT)
	                | lcuL_maskflag(udp, FLAG_DGRMMASK);
}

#define addrbinsz(O) lcuL_maskflag(O, FLAG_IPV6DOM) ? sizeof(struct in6_addr) \
                                                    : sizeof(struct in_addr)

LCULIB_API void *lcu_getudpmcastiface (lcu_UdpSocket *udp, size_t *sz) {
	if (sz) *sz = addrbinsz(udp);
	return (void *)&udp->mcastiface;
}

LCULIB_API int lcu_setudpmcastiface (lcu_UdpSocket *udp, const void *data, size_t sz) {
	size_t expected = addrbinsz(udp);
	if (sz == expected) {
		memcpy((void *)&udp->mcastiface, data, sz);
		return 0;
	}
	return UV_EINVAL;
}

LCULIB_API int lcu_gettcpkeepalive (lcu_TcpSocket *tcp) {
	if (lcuL_maskflag(tcp, FLAG_KEEPALIVE)) {
		return tcp->flags>>KEEPALIVE_SHIFT;
	}
	return -1;
}

LCULIB_API void lcu_settcpkeepalive (lcu_TcpSocket *tcp, int delay) {
	if (delay < 0) lcuL_clearflag(tcp, FLAG_KEEPALIVE);
	else tcp->flags = (delay<<KEEPALIVE_SHIFT)
	                | FLAG_KEEPALIVE
	                | (tcp->flags&FLAG_STRMMASK);
}
