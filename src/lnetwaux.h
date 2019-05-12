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


#define LCU_UDPSOCKCLS LCU_PREFIX"udpsocket"

typedef struct lcu_UdpSocket lcu_UdpSocket;

#define lcu_checkudp(L,i)	((lcu_UdpSocket *) \
                         	 loopL_checkinstance(L, i, LCU_UDPSOCKCLS))

#define lcu_toudp(L,i)	((lcu_UdpSocket *) \
                      	 loopL_testinstance(L, i, LCU_UDPSOCKCLS))

#define lcu_isudp(L,i)	(lcu_toudp(L, i) != NULL)

LCULIB_API lcu_UdpSocket *lcu_newudp (lua_State *L, int domain);

LCULIB_API void lcu_enableudp (lua_State *L, int idx);

LCULIB_API uv_udp_t *lcu_toudphandle (lcu_UdpSocket *udp);

LCULIB_API int lcu_isudpclosed (lcu_UdpSocket *udp);

LCULIB_API int lcu_closeudp (lua_State *L, int idx);

LCULIB_API int lcu_getudparmed (lcu_UdpSocket *udp);

LCULIB_API void lcu_setudparmed (lcu_UdpSocket *udp, int value);

LCULIB_API int lcu_getudpaddrfam (lcu_UdpSocket *udp);

LCULIB_API int lcu_getudpconnected (lcu_UdpSocket* udp);

LCULIB_API void lcu_setudpconnected (lcu_UdpSocket* udp, int enabled);

LCULIB_API int lcu_getudpbroadcast (lcu_UdpSocket* udp);

LCULIB_API void lcu_setudpbroadcast (lcu_UdpSocket* udp, int enabled);

LCULIB_API int lcu_getudpmcastloop (lcu_UdpSocket* udp);

LCULIB_API void lcu_setudpmcastloop (lcu_UdpSocket* udp, int enabled);

LCULIB_API int lcu_getudpmcastttl (lcu_UdpSocket *udp);

LCULIB_API void lcu_setudpmcastttl (lcu_UdpSocket *udp, int value);

LCULIB_API void *lcu_getudpmcastiface (lcu_UdpSocket *udp, size_t *sz);

LCULIB_API int lcu_setudpmcastiface (lcu_UdpSocket *udp, const void *data, size_t sz);

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

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int domain, int class);

LCULIB_API void lcu_enabletcp (lua_State *L, int idx);

LCULIB_API uv_tcp_t *lcu_totcphandle (lcu_TcpSocket *tcp);

LCULIB_API int lcu_istcpclosed (lcu_TcpSocket *tcp);

LCULIB_API int lcu_closetcp (lua_State *L, int idx);

LCULIB_API int lcu_gettcparmed (lcu_TcpSocket *tcp);

LCULIB_API void lcu_settcparmed (lcu_TcpSocket *tcp, int value);

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
