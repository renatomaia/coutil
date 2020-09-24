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


#endif