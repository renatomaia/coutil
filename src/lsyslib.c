#include "lsyslib.h"
#include "loperaux.h"


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
	na = (struct sockaddr *)lua_newuserdatauv(L, sz, 0);
	memset(na, 0, sz);
	luaL_setmetatable(L, LCU_NETADDRCLS);
	return na;
}


/*
 * Names
 */

struct lcu_AddressList {
	struct addrinfo *start;
	struct addrinfo *current;
};

LCULIB_API lcu_AddressList *lcu_newaddrlist (lua_State *L) {
	lcu_AddressList *l =
		(lcu_AddressList *)lua_newuserdatauv(L, sizeof(lcu_AddressList), 0);
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

#define FLAG_LISTEN 0x08  /* only for passive streams */

#define FLAG_NODELAY 0x08  /* only for active TCP sockets */
#define FLAG_KEEPALIVE 0x10  /* only for active TCP sockets */

#define FLAG_CONNECTED 0x08  /* only for UDP sockets */
#define FLAG_BROADCAST 0x10  /* only for UDP sockets */
#define FLAG_MCASTLOOP 0x20  /* only for UDP sockets */
#define FLAG_MCASTTTL 0x3fd0  /* only for UDP sockets */

#define FLAG_PIPEPERM 0x18  /* only for pipe sockets */

#define FLAG_LSTNMASK 0x0f  /* only for passive streams */
#define FLAG_TCPMASK 0x1f  /* only for active TCP sockets */
#define FLAG_UDPMASK 0x3f  /* only for UDP sockets */
#define FLAG_PIPEMASK 0x07  /* only for pipe sockets */

#define LISTENCONN_UNIT (FLAG_LSTNMASK+1)
#define KEEPALIVE_SHIFT 5
#define MCASTTTL_SHIFT 6
#define PIPEPERM_SHIFT 3

struct lcu_Object {
	int flags;
	lcu_ObjectAction stop;
	lua_CFunction step;
	uv_handle_t handle;
};

struct lcu_UdpSocket {
	int flags;
	lcu_ObjectAction stop;
	lua_CFunction step;
	uv_udp_t handle;
	union {
		struct in_addr ipv4;
		struct in6_addr ipv6;
	} mcastiface;
};

struct lcu_TcpSocket {
	int flags;
	lcu_ObjectAction stop;
	lua_CFunction step;
	uv_tcp_t handle;
};

struct lcu_IpcPipe {
	int flags;
	lcu_ObjectAction stop;
	lua_CFunction step;
	uv_pipe_t handle;
};

static lcu_Object *createobj (lua_State *L, size_t size, const char *cls) {
	lcu_Object *object = (lcu_Object *)lua_newuserdatauv(L, size, 1);
	object->flags = FLAG_CLOSED;
	object->stop = NULL;
	object->step = NULL;
	object->handle.data = NULL;
	luaL_setmetatable(L, cls);
	return object;
}

LCULIB_API int lcuT_closeobj (lua_State *L, int idx) {
	lcu_Object *object = (lcu_Object *)lua_touserdata(L, idx);
	if (object && !lcuL_maskflag(object, FLAG_CLOSED)) {
		lcuT_closeobjhdl(L, idx, lcu_toobjhdl(object));
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

LCULIB_API lcu_ObjectAction lcu_getobjstop (lcu_Object *object) {
	return object->stop;
}

LCULIB_API void lcu_setobjstop (lcu_Object *object, lcu_ObjectAction value) {
	object->stop = value;
}

LCULIB_API lua_CFunction lcu_getobjstep (lcu_Object *object) {
	return object->step;
}

LCULIB_API void lcu_setobjstep (lcu_Object *object, lua_CFunction value) {
	object->step = value;
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
	                                                   LCU_UDPSOCKETCLS);
	if (domain == AF_INET6) lcuL_setflag(udp, FLAG_IPV6DOM);
	lcuL_setflag(udp, 1<<MCASTTTL_SHIFT);
	return udp;
}

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L,
                                      const char *class,
                                      int domain) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)createobj(L, sizeof(lcu_TcpSocket),
	                                                   class);
	if (domain == AF_INET6) lcuL_setflag(tcp, FLAG_IPV6DOM);
	return tcp;
}

LCULIB_API lcu_IpcPipe *lcu_newpipe (lua_State *L,
                                     const char *class,
                                     int ipc) {
	lcu_IpcPipe *pipe = (lcu_IpcPipe *)createobj(L, sizeof(lcu_IpcPipe), class);
	if (ipc) lcuL_setflag(pipe, FLAG_IPV6DOM);
	return pipe;
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
	                | lcuL_maskflag(udp, FLAG_UDPMASK);
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
	                | (tcp->flags&FLAG_TCPMASK);
}

LCULIB_API int lcu_getpipeperm (lcu_IpcPipe *pipe) {
	return lcuL_maskflag(pipe, FLAG_PIPEPERM)>>PIPEPERM_SHIFT;
}

LCULIB_API void lcu_setpipeperm (lcu_IpcPipe *pipe, int value) {
	pipe->flags = (FLAG_PIPEPERM&(value<<PIPEPERM_SHIFT))
	            | lcuL_maskflag(pipe, FLAG_PIPEMASK);
}
