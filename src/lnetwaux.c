#include "lnetwaux.h"


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

LCULIB_API lcu_TcpSocket *lcu_createtcp (lua_State *L, int domain, int class) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)lua_newuserdata(L, sizeof(lcu_TcpSocket));
	tcp->flags = domain == AF_INET6 ? LCU_TCPIPV6DOMAIN_FLAG : 0;
	tcp->ka_delay = 0;
	tcp->handle.data = (void *)LUA_REFNIL;  /* mark as closed */
	luaL_setmetatable(L, lcu_SocketClasses[class]);
	return tcp;
}

LCULIB_API void lcu_enabletcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx);
	tcp->flags = domain == AF_INET6 ? LCU_TCPIPV6DOMAIN_FLAG : 0;
	tcp->ka_delay = 0;
	tcp->handle.data = (void *)LUA_NOREF;  /* mark as not closed */
	luaL_setmetatable(L, lcu_SocketClasses[class]);
	return tcp;
}

static void lcuB_ontcpclosed (uv_handle_t *h) {
	lua_State *L = (lua_State *)h->loop->data;
	luaL_unref(L, (int)h->data);  /* let it become garbage */
}

LCULIB_API int lcu_closetcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx);
	if (tcp && lcu_islivetcp(tcp)) {
		tcp->handle.data = (void *)luaL_ref(L, idx);  /* save and mark as closed */
		uv_close(&tcp->handle, lcuB_ontcpclosed);
		return 1;
	}
	return 0;
}

