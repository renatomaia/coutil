#include "lprocaux.h"
#include "loperaux.h"

#include <string.h>


#define FLAG_CLOSED 0x01
#define FLAG_EXITED 0x02

struct lcu_Process {
	uv_process_t handle;
	int flags;
	int64_t exitval;
	int signal;
};

LCULIB_API lcu_Process *lcu_newprocess (lua_State *L) {
	lcu_Process *process = (lcu_Process *)lua_newuserdata(L, sizeof(lcu_Process));
	process->flags = FLAG_CLOSED;
	luaL_setmetatable(L, LCU_PROCESSCLS);
	return process;
}

LCULIB_API void lcu_enableproc (lua_State *L, int idx) {
	lcu_Process *process = lcu_toprocess(L, idx);
	lcu_assert(lcuL_maskflag(process, FLAG_CLOSED));
	lcu_assert(process->handle.data == NULL);
	lcuL_clearflag(process, FLAG_CLOSED);
}

LCULIB_API uv_process_t *lcu_toprochandle (lcu_Process *process) {
	return &process->handle;
}

LCULIB_API int lcu_isprocclosed (lcu_Process *process) {
	return lcuL_maskflag(process, FLAG_CLOSED);
}

LCULIB_API int lcu_closeproc (lua_State *L, int idx) {
	lcu_Process *process = lcu_toprocess(L, idx);
	if (process && !lcu_isprocclosed(process)) {
		lcu_closeobj(L, idx, (uv_handle_t *)&process->handle);
		lcuL_clearflag(process, FLAG_EXITED);
		lcuL_setflag(process, FLAG_CLOSED);
		return 1;
	}
	return 0;
}

LCULIB_API int lcu_getprocexited (lcu_Process *process,
                                  int64_t *exitval,
                                  int *signal) {
	if (!lcuL_maskflag(process, FLAG_EXITED)) return 0;
	if (exitval) *exitval = process->exitval;
	if (signal) *signal = process->signal;
	return 1;
}

LCULIB_API void lcu_setprocexited (lcu_Process *process,
                                   int64_t exitval,
                                   int signal) {
	lcuL_setflag(process, FLAG_EXITED);
	process->exitval = exitval;
	process->signal = signal;
}
