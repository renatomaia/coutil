#include "lnetwaux.h"

#include <string.h>


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

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class) {
	lcu_TcpSocket *tcp = (lcu_TcpSocket *)lua_newuserdata(L, sizeof(lcu_TcpSocket));
	tcp->flags = LCU_TCPFLAG_CLOSING|(domain==AF_INET6 ? LCU_TCPFLAG_IPV6DOM : 0);
	tcp->handle.data = (void *)LUA_REFNIL;  /* not stored in registry */
	tcp->ka_delay = 0;
	luaL_setmetatable(L, lcu_TcpSockCls[class]);
	return tcp;
}

LCULIB_API void lcu_enabletcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	lcu_assert(tcp && tcp->flags&LCU_TCPFLAG_CLOSING &&
	                  tcp->handle.data == (void *)LUA_REFNIL);
	tcp->flags &= ~LCU_TCPFLAG_CLOSING;
	tcp->handle.data = NULL;  /* mark as not in use */
}

static void lcuB_ontcpclosed (uv_handle_t *h) {
	lua_State *L = (lua_State *)h->loop->data;
	luaL_unref(L, LUA_REGISTRYINDEX, (intptr_t)h->data);  /* becomes garbage */
}

LCULIB_API int lcu_closetcp (lua_State *L, int idx) {
	lcu_TcpSocket *tcp = lcu_totcp(L, idx, LCU_TCPTYPE_SOCKET);
	if (tcp && lcu_islivetcp(tcp)) {
		lua_pushvalue(L, idx);
		tcp->handle.data = (void *)(intptr_t)luaL_ref(L, LUA_REGISTRYINDEX);
		tcp->flags |= LCU_TCPFLAG_CLOSING;
		uv_close((uv_handle_t *)&tcp->handle, lcuB_ontcpclosed);
		return 1;
	}
	return 0;
}

