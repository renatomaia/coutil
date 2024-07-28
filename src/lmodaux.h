#ifndef lmodaux_h
#define lmodaux_h


#include "lcuconf.h"

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>


#define LCU_TASKTPOOLREGKEY	LCU_PREFIX"ThreadPool *taskThreadPool"
#define LCU_CHANNELTASKREGKEY	LCU_PREFIX"ChannelTask channelTask"
#define LCU_CHANNELSREGKEY	LCU_PREFIX"ChannelMap channelMap"
#define LCU_STDIOFDREGKEY	LCU_PREFIX"int stdiofd[3]"


#define lcu_time2sec(T)	((T).tv_sec+((lua_Number)((T).tv_usec)*1e-6))
#define lcu_ntime2sec(T)	((T).tv_sec+((lua_Number)((T).tv_nsec)*1e-9))

#define lcu_error(L,e)	luaL_error(L, uv_strerror(e))

#define lcu_pusherror(L,e)	lua_pushstring(L, uv_strerror(e))

LCUI_FUNC int lcuL_pusherrres (lua_State *L, int err);

LCUI_FUNC int lcuL_pushresults (lua_State *L, int n, int err);

LCUI_FUNC void lcuL_warnmsg (lua_State *L, const char *prefix, const char *msg);

LCUI_FUNC void lcuL_warnerr (lua_State *L, const char *prefix, int err);

LCUI_FUNC void lcuL_setfinalizer (lua_State *L, lua_CFunction finalizer);

LCUI_FUNC void lcu_getinputbuf (lua_State *L, int arg, uv_buf_t *buf);

LCUI_FUNC void lcu_getoutputbuf (lua_State *L, int arg, uv_buf_t *buf);

typedef int (*lcu_GetStringFunc) (char *buffer, size_t *len);

LCUI_FUNC void lcu_pushstrout(lua_State *L, lcu_GetStringFunc getter);

#define lcuL_maskflag(O,F) ((O)->flags&(F))
#define lcuL_setflag(O,F) ((O)->flags |= (F))
#define lcuL_clearflag(O,F) ((O)->flags &= ~(F))

LCUI_FUNC lua_State *lcuL_newstate (lua_State *L);

LCUI_FUNC lua_State *lcuL_tomain (lua_State *L);

LCUI_FUNC int lcuL_canmove (lua_State *L,
                            int n,
                            const char *msg);

LCUI_FUNC int lcuL_pushfrom (lua_State *L,
                             lua_State *to,
                             lua_State *from,
                             int idx,
                             const char *msg);

LCUI_FUNC int lcuL_movefrom (lua_State *L,
                             lua_State *to,
                             lua_State *from,
                             int n,
                             const char *msg);

LCUI_FUNC void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup);

LCUI_FUNC void lcuL_printstack (uv_thread_t tid,
                                lua_State *L,
                                const char *file,
                                int line,
                                const char *func);



#endif
