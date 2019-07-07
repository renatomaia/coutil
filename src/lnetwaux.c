#include "lnetwaux.h"
#include "loperaux.h"

#include <string.h>

#define newbooloption(name, type, flag) \
	LCULIB_API int lcu_get##name(type *obj) { \
		return lcuL_maskflag(obj, flag); \
	} \
	\
	LCULIB_API void lcu_set##name(type *obj, int enabled) { \
		if (enabled) lcuL_setflag(obj, flag); \
		else lcuL_clearflag(obj, flag); \
	}


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

#define FLAG_IPV6DOM 0x01
#define FLAG_CLOSED 0x02
#define FLAG_OBJOPON 0x04

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

struct lcu_UdpSocket {
	uv_udp_t handle;
	int flags;
	union {
		struct in_addr ipv4;
		struct in6_addr ipv6;
	} mcastiface;
};

struct lcu_TcpSocket {
	uv_tcp_t handle;
	int flags;
};

#define initsockobj(O,D,F) O->handle.data = NULL; \
                         if (D == AF_INET) O->flags = FLAG_CLOSED|(F); \
                         else O->flags = FLAG_CLOSED|FLAG_IPV6DOM|(F);

LCULIB_API lcu_UdpSocket *lcu_newudp (lua_State *L, int domain) {
	lcu_UdpSocket *udp = (lcu_UdpSocket *)lua_newuserdata(L, sizeof(lcu_UdpSocket));
	initsockobj(udp, domain, 1<<MCASTTTL_SHIFT);
	luaL_setmetatable(L, LCU_UDPSOCKCLS);
	return udp;
}

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)lua_newuserdata(L, sizeof(lcu_TcpSocket));
	initsockobj(tcp, domain, 0);
	luaL_setmetatable(L, lcu_TcpSockCls[class]);
	return tcp;
}

LCULIB_API void lcu_enableudp (lua_State *L, int idx) {
	lcu_UdpSocket *udp = lcu_toudp(L, idx);
	lcu_assert(lcuL_maskflag(udp, FLAG_CLOSED));
	lcu_assert(udp->handle.data == NULL);
	lcuL_clearflag(udp, FLAG_CLOSED);
}

LCULIB_API void lcu_enabletcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	lcu_assert(lcuL_maskflag(tcp, FLAG_CLOSED));
	lcu_assert(tcp->handle.data == NULL);
	lcuL_clearflag(tcp, FLAG_CLOSED);
}

LCULIB_API uv_udp_t *lcu_toudphandle (lcu_UdpSocket *udp) {
	return &udp->handle;
}

LCULIB_API uv_tcp_t *lcu_totcphandle (lcu_TcpSocket *tcp) {
	return &tcp->handle;
}


LCULIB_API int lcu_isudpclosed (lcu_UdpSocket *udp) {
	return lcuL_maskflag(udp, FLAG_CLOSED);
}

LCULIB_API int lcu_istcpclosed (lcu_TcpSocket *tcp) {
	return lcuL_maskflag(tcp, FLAG_CLOSED);
}

LCULIB_API int lcu_closeudp (lua_State *L, int idx) {
	lcu_UdpSocket *udp = lcu_toudp(L, idx);
	if (udp && !lcu_isudpclosed(udp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&udp->handle);
		lcuL_clearflag(udp, FLAG_LISTEN);
		lcuL_setflag(udp, FLAG_CLOSED);
		return 1;
	}
	return 0;
}

LCULIB_API int lcu_closetcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	if (tcp && !lcu_istcpclosed(tcp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&tcp->handle);
		lcuL_clearflag(tcp, FLAG_LISTEN);
		lcuL_setflag(tcp, FLAG_CLOSED);
		return 1;
	}
	return 0;
}

LCULIB_API int lcu_getudpaddrfam (lcu_UdpSocket *udp) {
	return lcuL_maskflag(udp, FLAG_IPV6DOM) ? AF_INET6 : AF_INET;
}

LCULIB_API int lcu_gettcpaddrfam (lcu_TcpSocket *tcp) {
	return lcuL_maskflag(tcp, FLAG_IPV6DOM) ? AF_INET6 : AF_INET;
}

newbooloption(udparmed, lcu_UdpSocket, FLAG_OBJOPON);
newbooloption(tcparmed, lcu_TcpSocket, FLAG_OBJOPON);

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

LCULIB_API void lcu_addtcplisten (lcu_TcpSocket *tcp) {
	tcp->flags += LISTENCONN_UNIT;
}

LCULIB_API int lcu_picktcplisten (lcu_TcpSocket *tcp) {
	if (tcp->flags < 2*LISTENCONN_UNIT) return 0;
	tcp->flags -= LISTENCONN_UNIT;
	return 1;
}

LCULIB_API int lcu_istcplisten (lcu_TcpSocket *tcp) {
	return tcp->flags >= LISTENCONN_UNIT;
}

LCULIB_API void lcu_marktcplisten (lcu_TcpSocket *tcp) {
	if (!lcu_istcplisten(tcp)) lcu_addtcplisten(tcp);
}
