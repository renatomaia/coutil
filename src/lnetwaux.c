#include "lnetwaux.h"

#include <string.h>


#define FLAG_KEEPALIVE 0x01
#define FLAG_NODELAY 0x02
#define FLAG_IPV6DOM 0x04
#define FLAG_CLOSING 0x08

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
	tcp->flags = FLAG_CLOSING;
	if (domain == AF_INET6) lcuL_setflag(tcp, FLAG_IPV6DOM);
	luaL_setmetatable(L, lcu_TcpSockCls[class]);
	return tcp;
}

LCULIB_API void lcu_enabletcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, TYPE_SOCKET);
	lcu_assert(lcuL_hasflag(tcp, FLAG_CLOSING));
	lcu_assert(tcp->handle.data == NULL);
	lcuL_clearflag(tcp, FLAG_CLOSING);
}

LCULIB_API int lcu_islivetcp (lcu_TcpSocket *tcp) {
	return !lcuL_hasflag(tcp, FLAG_CLOSING);
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
		return tcp->ka_delay;
	}
	return -1;
}

LCULIB_API void lcu_settcpkeepalive (lcu_TcpSocket *tcp, int delay) {
	if (delay < 0) {
		lcuL_clearflag(tcp, FLAG_KEEPALIVE);
	} else {
		lcuL_setflag(tcp, FLAG_KEEPALIVE);
		tcp->ka_delay = delay;
	}
}

LCULIB_API int lcu_closetcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	if (tcp && lcu_islivetcp(tcp)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&tcp->handle);
		lcuL_setflag(tcp, FLAG_CLOSING);
		return 1;
	}
	return 0;
}

