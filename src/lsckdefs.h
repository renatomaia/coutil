#ifndef lsckdefs_h
#define lsckdefs_h


#include "loperaux.h"

#ifndef _WIN32
#include <netinet/in.h>  /* network addresses */
#include <arpa/inet.h>  /* IP addresses */
#else
#include <winsock2.h>
typedef unsigned short in_port_t;
#endif


typedef struct lcu_UdpSocket {
	int flags;
	lcu_HandleAction stop;
	lua_CFunction step;
	uv_udp_t handle;
} lcu_UdpSocket;

typedef struct lcu_TcpSocket {
	int flags;
	lcu_HandleAction stop;
	lua_CFunction step;
	uv_tcp_t handle;
} lcu_TcpSocket;

typedef struct lcu_PipeSocket {
	int flags;
	lcu_HandleAction stop;
	lua_CFunction step;
	uv_pipe_t handle;
} lcu_PipeSocket;

typedef struct lcu_TermSocket {
	int flags;
	lcu_HandleAction stop;
	lua_CFunction step;
	uv_tty_t handle;
} lcu_TermSocket;


#define LCU_SOCKIPV6FLAG 0x02  /* used for network sockets */
#define LCU_SOCKTRANFFLAG 0x02  /* used for local|share sockets */


#endif
