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

#define lcu_checkaddress(L,i)  ((struct sockaddr *) \
                                luaL_checkudata(L, i, LCU_NETADDRCLS))

#define lcu_toaddress(L,i)  ((struct sockaddr *) \
                             luaL_testudata(L, i, LCU_NETADDRCLS))

#define lcu_isaddress(L,i)  (lcu_toaddress(L, i) != NULL)

LCULIB_API struct sockaddr *lcu_newaddress (lua_State *L, int type);


/* superclasses used only in Lua */
typedef enum lcu_TcpSockType {
	LCU_TCPTYPE_STREAM = 0,
	LCU_TCPTYPE_LISTEN,
	LCU_TCPTYPE_SOCKET
} losi_TcpSockType;

static const char *const lcu_TcpSockCls[] = {
	LCU_PREFIX"tcpstream",
	LCU_PREFIX"tcplisten",
	LCU_PREFIX"tcpsocket"
};

typedef struct lcu_TcpSocket lcu_TcpSocket;

#define lcu_checktcp(L,i,c)	((lcu_TcpSocket *) \
                           	 loopL_checkinstance(L, i, lcu_TcpSockCls[c]))

#define lcu_totcp(L,i,c)	((lcu_TcpSocket *) \
                        	 loopL_testinstance(L, i, lcu_TcpSockCls[c]))

#define lcu_istcp(L,i,c)	(lcu_totcp(L, i, c) != NULL)

LCULIB_API uv_tcp_t *lcu_totcphandle (lcu_TcpSocket *tcp);

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class);

LCULIB_API void lcu_enabletcp (lua_State *L, int idx);

LCULIB_API int lcu_closetcp (lua_State *L, int idx);

LCULIB_API int lcu_islivetcp (lcu_TcpSocket *tcp);

LCULIB_API int lcu_gettcpaddrfam (lcu_TcpSocket *tcp);

LCULIB_API int lcu_gettcpnodelay (lcu_TcpSocket *tcp);

LCULIB_API void lcu_settcpnodelay (lcu_TcpSocket *tcp, int on);

LCULIB_API int lcu_gettcpkeepalive (lcu_TcpSocket *tcp);

LCULIB_API void lcu_settcpkeepalive (lcu_TcpSocket *tcp, int delay);

LCULIB_API void lcu_addtcplisten (lcu_TcpSocket *tcp);

LCULIB_API int lcu_picktcplisten (lcu_TcpSocket *tcp);

LCULIB_API int lcu_istcplisten (lcu_TcpSocket *tcp);

LCULIB_API void lcu_marktcplisten (lcu_TcpSocket *tcp);

#endif
