#ifndef lnetwaux_h
#define lnetwaux_h


#include "lcuconf.h"

#include <lua.h>
#include <lauxlib.h>


#define LCU_NETADDRCLS LCU_PREFIX"NetworkAddress"

#define lcu_chkaddress(L,i)	((struct sockaddr *)luaL_checkudata(L, i, \
                           	LCU_NETADDRCLS))

#define lcu_toaddress(L,i)	((struct sockaddr *)luaL_testudata(L, i, \
                          	LCU_NETADDRCLS))

#define lcu_isaddress(L,i)	(lcu_toaddress(L, i) != NULL)

LCULIB_API struct sockaddr *lcu_newaddress (lua_State *L, int type);


/* superclasses used only in Lua */
typedef enum lcu_SocketType {
	LCU_SOCKTYPE_LISTEN = 0,
	LCU_SOCKTYPE_STREAM,
	LCU_SOCKTYPE_DGRM
} losi_SocketType;
#define LCU_SOCKTYPE_TRSP 3
#define LCU_SOCKTYPE_SOCK 4

static const char *const lcu_SocketClasses[] = {
	LCU_PREFIX"ListenSocket",
	LCU_PREFIX"StreamSocket",
	LCU_PREFIX"DatagramSocket",
	LCU_PREFIX"TransportSocket",
	LCU_PREFIX"NetworkSocket"
};

#define LCU_TCPKEEPALIVE_FLAG 0x01
#define LCU_TCPNODELAY_FLAG 0x02
#define LCU_TCPIPV6DOM_FLAG 0x04

typedef struct lcu_Socket {
	uv_os_sock_t socket;
	int flags;
	int ka_delay;
} lcu_Socket;

LCULIB_API lcu_Socket *lcu_newsocket (lua_State *L, int class);

#define lcu_islivetcp(p)	((p)->handle.data != (void *)LUA_NOREF)

#define lcu_gettcpdom(p)	((p)->flags&LCU_TCPIPV6DOM_FLAG : AF_INET6 : AF_INET)

#define lcu_chktcp(L,i,c)	((lcu_TcpSocket *)luaL_testudata(L, i, \
                         	lcu_TcpClasses[c]))

#define lcu_totcp(L,i,c)	((lcu_TcpSocket *)luaL_testudata(L, i, \
                        	lcu_TcpClasses[c]))

#define lcu_istcp(L,i,c)	(lcu_totcp(L, i, c) != NULL)


#endif
