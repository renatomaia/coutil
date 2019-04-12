#include "lmodaux.h"
#include "lhndlaux.h"


#define LCU_COREGISTRY	lua_upvalueindex(2)
#define LCU_HANDLEMAP	lua_upvalueindex(3)


LCULIB_API uv_handle_t *lcu_tohandle (lua_State *L) {
	uv_handle_t *h;
	lua_pushthread(L);
	if (lua_gettable(L, LCU_HANDLEMAP) == LUA_TNIL) {
		lua_pushthread(L);
		h = (uv_handle_t *)lua_newuserdata(L, sizeof(union uv_any_handle));
		h->type = UV_UNKNOWN_HANDLE;
		h->data = NULL;
		lua_settable(L, LCU_HANDLEMAP);
	}
	else h = (uv_handle_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return h;
}


static void savehdlcoro (uv_handle_t *h, lua_State *L) {
	lua_pushlightuserdata(L, h);
	lua_pushthread(L);
	lua_settable(L, LCU_COREGISTRY);
}

static void discardhdlcoro (uv_handle_t *h) {
	lua_State *L = (lua_State *)h->loop->data;
	lua_pushlightuserdata(L, h);
	lua_pushnil(L);
	lua_settable(L, LCU_COREGISTRY);
}

static void lcuB_onclosed (uv_handle_t *h) {
	h->type = UV_UNKNOWN_HANDLE;
	lua_State *co = (lua_State *)h->data;
	if (co != NULL) lcu_resumehdl(h, co);
	else discardhdlcoro(h);
}

LCULIB_API void lcu_chkinithdl (lua_State *L, uv_handle_t *h, int err) {
	lcu_chkerror(L, err);
	savehdlcoro(h, L);
}

LCULIB_API void lcu_chkstarthdl (lua_State *L, uv_handle_t *h, int err) {
	if (err < 0) {
		uv_close(h, lcuB_onclosed);
		lcu_error(L, err);  /* never returns */
	}
}


#define lcu_yieldhdl(L,n,c,f,h) ((h)->data = L, lua_yieldk(L, n, c, f))

LCULIB_API void lcu_resumehdl (uv_handle_t *h, lua_State *co) {
	lua_State *L = (lua_State *)h->loop->data;
	int status;
	h->data = NULL;  /* mark as not rescheduled */
	lua_pushlightuserdata(co, h->loop);  /* token to sign scheduler resume */
	status = lua_resume(co, L, 1);
	if ( (status != LUA_YIELD || h->data == NULL) &&  /* was not rescheduled */
	     h->type != UV_UNKNOWN_HANDLE &&  /* is still scheduled */
	     !uv_is_closing(h) ) uv_close(h, lcuB_onclosed);
}

LCULIB_API uv_handle_t *lcu_resethdl (lua_State *L, uv_handle_type t, int narg,
                                         lua_KContext ctx, lua_KFunction func) {
	uv_handle_t *h = lcu_tohandle(L);
	if (!lua_isyieldable(L)) luaL_error(L, "unable to yield");
	if (h->type == UV_UNKNOWN_HANDLE) {
		func(L, LUA_YIELD, ctx);  /* never returns */
	} else if ( h->type != t || uv_is_closing(h) ) {
		if (!uv_is_closing(h)) uv_close(h, lcuB_onclosed);
		lcu_yieldhdl(L, narg, ctx, func, h);  /* never returns */
	}
	return h;
}

LCULIB_API int lcuK_chkcancelhdl (lua_State *L, int status, lua_KContext ctx) {
	uv_loop_t *loop = lcu_toloop(L);
	uv_handle_t *h = lcu_tohandle(L);
	int narg = (int)ctx;
	int cancel = lua_touserdata(L, -1) != loop;
	lcu_assert(status == LUA_YIELD);
	if (cancel && !uv_is_closing(h)) uv_close(h, lcuB_onclosed);
	h->data = NULL;  /* mark as not rescheduled */
	lua_pushboolean(L, !cancel);
	lua_insert(L, narg+1);
	return lua_gettop(L)-narg;
}
