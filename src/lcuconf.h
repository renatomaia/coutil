#include "lmodaux.h"




#ifndef lcuconf_h
#define lcuconf_h


#ifndef LCULIB_API
#define LCULIB_API
#endif

#ifndef LCU_PREFIX
#define LCU_PREFIX	"coutil:"
#endif

#ifndef LCU_EXECARGCOUNT
#define LCU_EXECARGCOUNT	1023
#endif

#include <assert.h>
#define lcu_assert assert


#if !defined(lcu_assert)
#define lcu_assert(X)	((void)(X))
#endif


#endif