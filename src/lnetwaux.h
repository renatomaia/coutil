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


#define LCU_NETADDRLISTCLS LCU_PREFIX"netaddrlist"

typedef struct lcu_AddressList lcu_AddressList;

#define lcu_checkaddrlist(L,i)  ((lcu_AddressList *) \
                                luaL_checkudata(L, i, LCU_NETADDRLISTCLS))

#define lcu_toaddrlist(L,i)  ((lcu_AddressList *) \
                             luaL_testudata(L, i, LCU_NETADDRLISTCLS))

#define lcu_isaddrlist(L,i)  (lcu_toaddrlist(L, i) != NULL)

LCULIB_API lcu_AddressList *lcu_newaddrlist (lua_State *L);

LCULIB_API void lcu_setaddrlist (lcu_AddressList *list, struct addrinfo *addrs);

LCULIB_API struct addrinfo *lcu_getaddrlist (lcu_AddressList *list);

LCULIB_API struct addrinfo *lcu_peekaddrlist (lcu_AddressList *list);

LCULIB_API struct addrinfo *lcu_nextaddrlist (lcu_AddressList *list);


#define LCU_OBJECTCLS LCU_PREFIX"object"

typedef struct lcu_Object lcu_Object;

LCULIB_API int lcu_closeobj (lua_State *L, int idx, const char *cls);

LCULIB_API void lcu_enableobj (lcu_Object *object);

LCULIB_API int lcu_isobjclosed (lcu_Object *object);

LCULIB_API uv_handle_t *lcu_toobjhdl (lcu_Object *object);

LCULIB_API lcu_Object *lcu_tohdlobj (uv_handle_t *handle);

LCULIB_API int lcu_getobjarmed(lcu_Object *obj);

LCULIB_API void lcu_setobjarmed(lcu_Object *obj, int enabled);

LCULIB_API int lcu_getobjdomain (lcu_Object *obj);

LCULIB_API void lcu_addobjlisten (lcu_Object *object);

LCULIB_API int lcu_pickobjlisten (lcu_Object *object);

LCULIB_API int lcu_isobjlisten (lcu_Object *object);

LCULIB_API void lcu_markobjlisten (lcu_Object *object);


#define LCU_UDPSOCKCLS LCU_PREFIX"udpsocket"

typedef struct lcu_UdpSocket lcu_UdpSocket;

#define lcu_checkudp(L,i)	((lcu_UdpSocket *) \
                         	 luaL_checkudata(L, i, LCU_UDPSOCKCLS))

#define lcu_toudp(L,i)	((lcu_UdpSocket *) \
                      	 luaL_testudata(L, i, LCU_UDPSOCKCLS))

#define lcu_isudp(L,i)	(lcu_toudp(L, i) != NULL)

#define lcu_toudphdl(o)	((uv_udp_t *)lcu_toobjhdl((lcu_Object *)o))

LCULIB_API lcu_UdpSocket *lcu_newudp (lua_State *L, int domain);

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

#define lcu_totcphdl(o)	((uv_tcp_t *)lcu_toobjhdl((lcu_Object *)o))

LCULIB_API lcu_TcpSocket *lcu_newtcp (lua_State *L, int class, int domain);

LCULIB_API int lcu_gettcpnodelay (lcu_TcpSocket *tcp);

LCULIB_API void lcu_settcpnodelay (lcu_TcpSocket *tcp, int on);

LCULIB_API int lcu_gettcpkeepalive (lcu_TcpSocket *tcp);

LCULIB_API void lcu_settcpkeepalive (lcu_TcpSocket *tcp, int delay);


typedef enum lcu_IpcPipeType {
	LCU_PIPETYPE_STREAM = 0,
	LCU_PIPETYPE_LISTEN,
	LCU_PIPETYPE_OBJECT
} lcu_IpcPipeType;

static const char *const lcu_IpcPipeCls[] = {
	LCU_PREFIX"pipestream",
	LCU_PREFIX"pipelisten",
	LCU_PREFIX"pipeobject"
};

typedef struct lcu_IpcPipe lcu_IpcPipe;

#define lcu_checkpipe(L,i,c) ((lcu_IpcPipe *) \
                              loopL_checkinstance(L, i, lcu_IpcPipeCls[c]))

#define lcu_topipe(L,i,c)  ((lcu_IpcPipe *) \
                            loopL_testinstance(L, i, lcu_IpcPipeCls[c]))

#define lcu_ispipe(L,i,c)  (lcu_topipe(L, i, c) != NULL)

#define lcu_topipehdl(L,o)	((uv_pipe_t *)lcu_topipehdl(L, (lcu_Object *)o))

LCULIB_API lcu_IpcPipe *lcu_newpipe (lua_State *L, int class);

#endif
