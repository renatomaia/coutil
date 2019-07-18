#include "lmodaux.h"




#ifndef lcuconf_h
#define lcuconf_h


#ifndef LCULIB_API
#define LCULIB_API
#endif

#ifndef LCU_PREFIX
#define LCU_PREFIX	"coutil:"
#endif

#ifndef LCU_PIPEADDRBUF
#define LCU_PIPEADDRBUF	128
#endif

#ifndef LCU_EXECARGCOUNT
#define LCU_EXECARGCOUNT	1023
#endif

#ifndef LCU_NETHOSTNAMESZ
#ifndef NI_MAXHOST
#define LCU_NETHOSTNAMESZ	NI_MAXHOST
#else
#define LCU_NETHOSTNAMESZ	1025
#endif
#endif

#ifndef LCU_NETSERVNAMESZ
#ifndef NI_MAXSERT
#define LCU_NETSERVNAMESZ	NI_MAXSERV
#else
#define LCU_NETSERVNAMESZ	32
#endif
#endif

#include <assert.h>
#define lcu_assert assert


#if !defined(lcu_assert)
#define lcu_assert(X)	((void)(X))
#endif


#endif