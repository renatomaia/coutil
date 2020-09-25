#ifndef lcuconf_h
#define lcuconf_h


#include <luaconf.h>


#ifndef LCUI_FUNC
#define LCUI_FUNC LUAI_FUNC
#endif

#ifndef LCULIB_API
#define LCULIB_API LUALIB_API
#endif

#ifndef LCUMOD_API
#define LCUMOD_API LUAMOD_API
#endif

#ifndef LCU_PREFIX
#define LCU_PREFIX	"coutil:"
#endif

#ifndef LCU_WARNPREFIX
#define LCU_WARNPREFIX	"[coutil] "
#endif

#ifndef LCU_PIPEADDRBUF
#define LCU_PIPEADDRBUF	128
#endif

#ifndef LCU_EXECARGCOUNT
#define LCU_EXECARGCOUNT	1023
#endif

#ifndef LCU_NETHOSTNAMESZ
#ifndef NI_MAXHOST
#define LCU_NETHOSTNAMESZ	1025
#else
#define LCU_NETHOSTNAMESZ	NI_MAXHOST
#endif
#endif

#ifndef LCU_NETSERVNAMESZ
#ifndef NI_MAXSERT
#define LCU_NETSERVNAMESZ	32
#else
#define LCU_NETSERVNAMESZ	NI_MAXSERV
#endif
#endif

#include <assert.h>
#define lcu_assert assert
//#define lcu_assert(X) (printf("%s:%d: %s = %s\n", __FILE__, __LINE__, __func__, (X) ? "true" : "false"), assert(X))


#if !defined(lcu_assert)
#define lcu_assert(X)	((void)(X))
#endif


#define LCU_UDPSOCKETCLS	LCU_PREFIX"udp"
#define LCU_TCPACTIVECLS	LCU_PREFIX"tcpactive"
#define LCU_TCPPASSIVECLS	LCU_PREFIX"tcppassive"
#define LCU_PIPEIPCCLS	LCU_PREFIX"pipeipc"
#define LCU_PIPEACTIVECLS	LCU_PREFIX"pipeactive"
#define LCU_PIPEPASSIVECLS	LCU_PREFIX"pipepassive"
#define LCU_SYSCOROCLS	LCU_PREFIX"syscoro"
#define LCU_CHANNELCLS	LCU_PREFIX"channel"
#define LCU_THREADSCLS	LCU_PREFIX"threads"
#define LCU_TPOOLGCCLS	LCU_PREFIX"lcu_ThreadPool *"


#endif