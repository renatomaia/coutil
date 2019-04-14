#include "lnetwaux.h"
#include "loperaux.h"

#include <string.h>


#define FLAG_IPV6DOM 0x01
#define FLAG_CLOSED 0x02
#define FLAG_LISTEN 0x04  /* only for listen sockets */
#define FLAG_NODELAY 0x04  /* only for stream sockets */
#define FLAG_KEEPALIVE 0x08  /* only for stream sockets */

#define FLAG_LSTNMASK 0x07  /* only for listen sockets */
#define FLAG_STRMMASK 0x0f  /* only for stream sockets */

#define LISTENCONN_UNIT (FLAG_LSTNMASK+1)
#define KEEPALIVE_SHIFT 5

struct lcu_TcpSocket {
	uv_tcp_t handle;
	int flags;
	int ka_delay;
};

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

LCULIB_API uv_tcp_t *lcu_totcphandle (lcu_TcpSocket *tcp) {
	return &tcp->handle;
}

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)lua_newuserdata(L, sizeof(lcu_TcpSocket));
	tcp->handle.data = NULL;
	tcp->ka_delay = 0;
	tcp->flags = FLAG_CLOSED;
	if (domain == AF_INET6) lcuL_setflag(tcp, FLAG_IPV6DOM);
	luaL_setmetatable(L, lcu_TcpSockCls[class]);
	return tcp;
}

LCULIB_API void lcu_enabletcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	lcu_assert(lcuL_hasflag(tcp, FLAG_CLOSED));
	lcu_assert(tcp->handle.data == NULL);
	lcuL_clearflag(tcp, FLAG_CLOSED);
}

LCULIB_API int lcu_closetcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	if (tcp && lcu_islivetcp(tcp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&tcp->handle);
		lcuL_clearflag(tcp, FLAG_LISTEN);
		lcuL_setflag(tcp, FLAG_CLOSED);
		return 1;
	}
	return 0;
}

LCULIB_API int lcu_islivetcp (lcu_TcpSocket *tcp) {
	return !lcuL_hasflag(tcp, FLAG_CLOSED);
}

LCULIB_API int lcu_gettcpaddrfam (lcu_TcpSocket *tcp) {
	return lcuL_hasflag(tcp, FLAG_IPV6DOM) ? AF_INET6 : AF_INET;
}

LCULIB_API int lcu_gettcpnodelay (lcu_TcpSocket *tcp) {
	return lcuL_hasflag(tcp, FLAG_NODELAY);
}

LCULIB_API void lcu_settcpnodelay (lcu_TcpSocket *tcp, int enabled) {
	if (enabled) lcuL_setflag(tcp, FLAG_NODELAY);
	else lcuL_clearflag(tcp, FLAG_NODELAY);
}

LCULIB_API int lcu_gettcpkeepalive (lcu_TcpSocket *tcp) {
	if (lcuL_hasflag(tcp, FLAG_KEEPALIVE)) {
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

