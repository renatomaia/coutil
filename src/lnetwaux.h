#ifndef lnetwaux_h
#define lnetwaux_h


#include "lcuconf.h"

#include <lua.h>
#include <lauxlib.h>
#include <looplib.h>
#include <uv.h>
#include <netinet/in.h>  /* network addresses */
#include <arpa/inet.h>  /* IP addresses */


#define LCU_NETADDRCLS LCU_PREFIX"netaddress"

#define lcu_chkaddress(L,i)	((struct sockaddr *) \
                           	 luaL_checkudata(L, i, LCU_NETADDRCLS))

#define lcu_toaddress(L,i)	((struct sockaddr *) \
                          	 luaL_testudata(L, i, LCU_NETADDRCLS))

#define lcu_isaddress(L,i)	(lcu_toaddress(L, i) != NULL)

LCULIB_API struct sockaddr *lcu_newaddress (lua_State *L, int type);


/* superclasses used only in Lua */
typedef enum lcu_TcpSockType {
	LCU_TCPTYPE_STREAM = 0,
	LCU_TCPTYPE_LISTEN,
} losi_TcpSockType;
#define LCU_TCPTYPE_SOCKET 2

static const char *const lcu_TcpSockCls[] = {
	LCU_PREFIX"tcpstream",
	LCU_PREFIX"tcplisten",
	LCU_PREFIX"tcpsocket"
};

#define LCU_TCPFLAG_KEEPALIVE 0x01
#define LCU_TCPFLAG_NODELAY 0x02
#define LCU_TCPFLAG_IPV6DOM 0x04
#define LCU_TCPFLAG_CLOSING 0x08
#define LCU_TCPFLAG_FLAGMASK 0x0f

typedef struct lcu_TcpSocket {
	uv_tcp_t handle;
	int flags;
	int ka_delay;
} lcu_TcpSocket;

#define lcu_islivetcp(p)	!((p)->flags&LCU_TCPFLAG_CLOSING)

#define lcu_gettcpdom(p)	((p)->flags&LCU_TCPFLAG_IPV6DOM ? AF_INET6 : AF_INET)

#define lcu_checktcp(L,i,c)	((lcu_TcpSocket *) \
                           	 loopL_checkinstance(L, i, lcu_TcpSockCls[c]))

#define lcu_totcp(L,i,c)	((lcu_TcpSocket *) \
                        	 loopL_testinstance(L, i, lcu_TcpSockCls[c]))

#define lcu_istcp(L,i,c)	(lcu_totcp(L, i, c) != NULL)

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class);

LCULIB_API void lcu_enabletcp (lua_State *L, int idx);

LCULIB_API int lcu_closetcp (lua_State *L, int idx);

#endif
