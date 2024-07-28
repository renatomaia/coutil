#include "lmodaux.h"

#include <stdlib.h>
#include <lualib.h>
#include <luamem.h>


/*
 * Results and errors
 */

LCUI_FUNC int lcuL_pusherrres (lua_State *L, int err) {
	lua_pushboolean(L, 0);
	lcu_pusherror(L, err);
	lua_pushinteger(L, -err);
	return 3;
}

LCUI_FUNC int lcuL_pushresults (lua_State *L, int n, int err) {
	if (err < 0) {
		lua_pop(L, n);
		return lcuL_pusherrres(L, err);
	} else if (n == 0) {
		lua_pushboolean(L, 1);
		return 1;
	}
	return n;
}

LCUI_FUNC void lcuL_warnmsg (lua_State *L, const char *prefix, const char *msg) {
	lua_warning(L, LCU_WARNPREFIX, 1);
	lua_warning(L, prefix, 1);
	lua_warning(L, ": ", 1);
	lua_warning(L, msg ? msg : "(null)", 0);
}

LCUI_FUNC void lcuL_warnerr (lua_State *L, const char *prefix, int err) {
	lcuL_warnmsg(L, prefix, uv_strerror(err));
}

LCUI_FUNC void lcuL_setfinalizer (lua_State *L, lua_CFunction finalizer) {
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, finalizer);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
}


/*
 * Buffer
 */

static size_t posrelatI (lua_Integer pos, size_t len) {
	if (pos > 0) return (size_t)pos;
	else if (pos == 0) return 1;
	else if (pos < -(lua_Integer)len) return 1;
	else return len + (size_t)pos + 1;
}

static size_t getendpos (lua_State *L, int arg, lua_Integer def, size_t len) {
	lua_Integer pos = luaL_optinteger(L, arg, def);
	if (pos > (lua_Integer)len) return len;
	else if (pos >= 0) return (size_t)pos;
	else if (pos < -(lua_Integer)len) return 0;
	else return len+(size_t)pos+1;
}

static void setupbuf (lua_State *L, int arg, uv_buf_t *buf, char *data, size_t sz) {
	size_t start = posrelatI(luaL_optinteger(L, arg+1, 1), sz);
	size_t end = getendpos(L, arg+2, -1, sz);
	if (start <= end) buf->len = end-start+1;
	else buf->len = 0;
	buf->base = data+start-1;
}

LCUI_FUNC void lcu_getinputbuf (lua_State *L, int arg, uv_buf_t *buf) {
	size_t sz;
	const char *data = luamem_checkarray(L, arg, &sz);
	setupbuf(L, arg, buf, (char *)data, sz);
}

LCUI_FUNC void lcu_getoutputbuf (lua_State *L, int arg, uv_buf_t *buf) {
	size_t sz;
	char *data = luamem_checkmemory(L, arg, &sz);
	setupbuf(L, arg, buf, data, sz);
}


/*
 * String getter
 */

LCUI_FUNC void lcu_pushstrout(lua_State *L, lcu_GetStringFunc getter) {
	char array[UV_MAXHOSTNAMESIZE];
	char *buffer = array;
	size_t len = sizeof(array);
	int err = getter(buffer, &len);
	if (err == UV_ENOBUFS) {
		buffer = (char *)malloc(len*sizeof(char));
		err = getter(buffer, &len);
	}
	if (err >= 0) lua_pushstring(L, buffer);
	if (buffer != array) free(buffer);
	if (err < 0) lcu_error(L, err);
}


/*
 * Lua states and value copying
 */

static const luaL_Reg stdlibs[] = {
	{"_G", luaopen_base},
	{LUA_COLIBNAME, luaopen_coroutine},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_IOLIBNAME, luaopen_io},
	{LUA_OSLIBNAME, luaopen_os},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_MATHLIBNAME, luaopen_math},
	{LUA_UTF8LIBNAME, luaopen_utf8},
	{LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
	{LUA_BITLIBNAME, luaopen_bit32},
#endif
	{NULL, NULL}
};

static int writer (lua_State *L, const void *b, size_t size, void *B) {
	(void)L;
	luaL_addlstring((luaL_Buffer *) B, (const char *)b, size);
	return 0;
};

static void copylightud (lua_State *L, lua_State *NL, const void *field) {
	if (lua_getfield(L, LUA_REGISTRYINDEX, field) != LUA_TNIL) {
		lcu_assert(lua_touserdata(L, -1) != NULL);
		lua_pushlightuserdata(NL, lua_touserdata(L, -1));
		lua_setfield(NL, LUA_REGISTRYINDEX, field);
	}
	lua_pop(L, 1);
}

static void warnf (void *ud, const char *message, int tocont);

static int initluastate (lua_State *NL) {
	const luaL_Reg *lib;
	lua_State *L = lua_touserdata(NL, 1);
	int *warnstate = (int *)lua_newuserdatauv(NL, sizeof(int), 0);
	*warnstate = 0;  /* default is warnings off */
	luaL_ref(NL, LUA_REGISTRYINDEX);  /* make sure it won't be collected */
	lua_setwarnf(NL, warnf, warnstate);

	lua_settop(NL, 0);
	lua_newthread(NL);  /* thread to be used to execute code */

	copylightud(L, NL, LCU_CHANNELSREGKEY);  /* copy channel map reference */
	copylightud(L, NL, LCU_STDIOFDREGKEY);  /* copy duplicated stdio files */

	luaL_requiref(NL, LUA_LOADLIBNAME, luaopen_package, 0);
	luaL_getsubtable(NL, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);

	/* add standard libraries to 'package.preload' */
	for (lib = stdlibs; lib->func; lib++) {
		lua_pushcfunction(NL, lib->func);
		lua_setfield(NL, -2, lib->name);
	}

	/* copy 'package.preload' */
	lua_pushnil(L);  /* first key */
	while (lua_next(L, -2) != 0) {
		if (lua_isstring(L, -2) && lua_isfunction(L, -1)) {
			size_t len;
			const char *name = lua_tolstring(L, -2, &len);
			lua_pushlstring(NL, name, len);
			if (lua_iscfunction(L, -1)) {
				lua_CFunction loader = lua_tocfunction(L, -1);
				lua_pushcfunction(NL, loader);
			} else {
				luaL_Buffer b;
				int err;
				luaL_buffinit(NL, &b);
				err = lua_dump(L, writer, &b, 0);
				luaL_pushresult(&b);
				if (err) {
					lua_pop(NL, 1);
					lua_pushnil(NL);
				} else {
					size_t l;
					const char *bytecodes = lua_tolstring(NL, -1, &l);
					int status = luaL_loadbufferx(NL, bytecodes, l, NULL, "b");
					lcu_assert(status == LUA_OK);
					lua_remove(NL, -2);
				}
			}
			lua_settable(NL, -3);
		}
		lua_pop(L, 1);
	}
	lua_pop(NL, 2);  /* remove 'package' and 'LUA_PRELOAD_TABLE' */
	lua_pop(L, 1);  /* remove 'LUA_PRELOAD_TABLE' */

	return 1;  /* return the thread to be used to execute code */
}

LCUI_FUNC lua_State *lcuL_newstate (lua_State *L) {
	void *allocud;
	lua_Alloc allocf = lua_getallocf(L, &allocud);
	lua_CFunction panic = lua_atpanic(L, NULL);  /* changes panic function */
	lua_State *NL = lua_newstate(allocf, allocud);
	int status;

	lua_atpanic(L, panic);  /* restore panic function */
	lua_atpanic(NL, panic);

	lua_pushcfunction(NL, initluastate);
	lua_pushlightuserdata(NL, L);
	status = lua_pcall(NL, 1, 1, 0);
	if (status != LUA_OK) {
		if (lcuL_pushfrom(NULL, L, NL, -1, "error") != LUA_OK) {
			lcu_log(NULL, L, "failure: unable copy error of a new Lua state!");
			lcuL_warnmsg(L, "discarded error", lua_tostring(NL, -1));
		}
		lua_close(NL);
		lua_error(L);
	}

	return lua_tothread(NL, 1);  /* returns the thread instead of the created state */
}

LCUI_FUNC lua_State *lcuL_tomain (lua_State *L) {
	lua_State *main;
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	main = lua_tothread(L, -1);
	lua_pop(L, 1);
	lcu_assert(lua_tothread(main, 1) == L);
	return main;
}

#define doerrmsg(F,L,I,M,T) (I > 0 ? \
	F(L, "unable to transfer %s #%d (got %s)", M, I, T) : \
	F(L, "unable to transfer %s (got %s)", M, T))

LCUI_FUNC int lcuL_canmove (lua_State *L, int narg, const char *msg) {
	int i;
	for (i = -narg; i < 0; i++) {
		switch (lua_type(L, i)) {
			case LUA_TNIL:
			case LUA_TBOOLEAN:
			case LUA_TNUMBER:
			case LUA_TSTRING:
			case LUA_TLIGHTUSERDATA:
				break;
			default: {
				const char *tname = luaL_typename(L, i);
				doerrmsg(lua_pushfstring, L, 3+narg+i, msg, tname);
				return 0;
			}
		}
	}
	return 1;
}

static void pushfrom (lua_State *to,
                      lua_State *from,
                      int idx,
                      const char *msg) {
	int type = lua_type(from, idx);
	switch (type) {
		case LUA_TNIL: {
			lua_pushnil(to);
		} break;
		case LUA_TBOOLEAN: {
			lua_pushboolean(to, lua_toboolean(from, idx));
		} break;
		case LUA_TNUMBER: {
			if (lua_isinteger(from, idx)) lua_pushinteger(to, lua_tointeger(from, idx));
			else lua_pushnumber(to, lua_tonumber(from, idx));
		} break;
		case LUA_TSTRING: {
			size_t l;
			const char *s = lua_tolstring(from, idx, &l);
			lua_pushlstring(to, s, l);
		} break;
		case LUA_TLIGHTUSERDATA: {
			lua_pushlightuserdata(to, lua_touserdata(from, idx));
		} break;
		default: {
			const char *tname = luaL_typename(from, idx);
			doerrmsg(luaL_error, to, idx, msg, tname);
		}
	}
}

static int auxpushfrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int idx = lua_tointeger(to, 2);
	const char *msg = (const char *)lua_touserdata(to, 3);
	pushfrom(to, from, idx, msg);
	return 1;
}

#define state2normal(L) ((lua_status(L) == LUA_YIELD) ? lcuL_tomain(L) : L)

LCUI_FUNC int lcuL_pushfrom (lua_State *L,
                             lua_State *to,
                             lua_State *from,
                             int idx,
                             const char *msg) {
	int status;
	if (L == NULL) L = state2normal(to);
	lcu_assert(lua_status(L) == LUA_OK);
	if (!lua_checkstack(to, 4)) return LUA_ERRMEM;
	lua_pushcfunction(L, auxpushfrom);
	lua_pushlightuserdata(L, from);
	lua_pushinteger(L, idx);
	lua_pushlightuserdata(L, (void *)msg);
	status = lua_pcall(L, 3, 1, 0);
	if (to != L) lua_xmove(L, to, 1);
	return status;
}

static int auxmovefrom (lua_State *to) {
	lua_State *from = (lua_State *)lua_touserdata(to, 1);
	int n = lua_tointeger(to, 2);
	const char *msg = (const char *)lua_touserdata(to, 3);
	int top = lua_gettop(from);
	int idx;
	lua_settop(to, 0);
	luaL_checkstack(to, n, "too many values");
	for (idx = 1+top-n; idx <= top; idx++) pushfrom(to, from, idx, msg);
	return n;
}

#define EXTRA	3  /* slots for operations like 'lcuCS_dequeuestateq' */

LCUI_FUNC int lcuL_movefrom (lua_State *L,
                             lua_State *to,
                             lua_State *from,
                             int n,
                             const char *msg) {
	int status;
	if (L == NULL) L = state2normal(to);
	lcu_assert(lua_gettop(from) >= n);
	lcu_assert(lua_status(L) == LUA_OK);
	if (!lua_checkstack(L, n > 3 ? n+1+EXTRA : 4+EXTRA) || !lua_checkstack(to, n+EXTRA))
		return LUA_ERRMEM;
	lua_pushcfunction(L, auxmovefrom);
	lua_pushlightuserdata(L, from);
	lua_pushinteger(L, n);
	lua_pushlightuserdata(L, (void *)msg);
	status = lua_pcall(L, 3, n, 0);
	if (status == LUA_OK) lua_pop(from, n);
	if (to != L) lua_xmove(L, to, status == LUA_OK ? n : 1);
	return status;
}


/*
 * Lua module creation
 */

LCUI_FUNC void lcuM_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -(nup+1));
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -2, l->name);
	}
}


/*
 * Debugging
 */

LCUI_FUNC void lcuL_printstack (uv_thread_t tid,
                                lua_State *L,
                                const char *file,
                                int line,
                                const char *func) {
	int i;
	printf("[%lx]%s:%d: function '%s'\n", tid, file, line, func);
	for(i = 1; i <= lua_gettop(L); i++) {
		const char *typename = NULL;
		printf("\t[%d] = ", i);
		switch (lua_type(L, i)) {
			case LUA_TNUMBER:
				printf("%g", lua_tonumber(L, i));
				break;
			case LUA_TSTRING:
				printf("\"%s\"", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf(lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNIL:
				printf("nil");
				break;
			case LUA_TUSERDATA:
				if (lua_getmetatable(L, i)) {
					lua_getfield(L, -1, "__name");
					typename = lua_tostring(L, -1);
					lua_pop(L, 2);
				}
				/* FALLTHRU */
			default:
				printf("%s: %p", typename ? typename : luaL_typename(L, i),
				                 lua_topointer(L, i));
				break;
		}
		printf("\n");
	}
	printf("\n");
}


/******************************************************************************
* The code below is copied from the source of Lua 5.4.0 by
* R. Ierusalimschy, L. H. de Figueiredo, W. Celes - Lua.org, PUC-Rio.
*
* Copyright (C) 1994-2020 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#include <string.h>

static void warnf (void *ud, const char *message, int tocont) {
	int *warnstate = (int *)ud;
	if (*warnstate != 2 && !tocont && *message == '@') {  /* control message? */
		if (strcmp(message, "@off") == 0)
			*warnstate = 0;
		else if (strcmp(message, "@on") == 0)
			*warnstate = 1;
		return;
	}
	else if (*warnstate == 0)  /* warnings off? */
		return;
	if (*warnstate == 1)  /* previous message was the last? */
		lua_writestringerror("%s", "Lua warning: ");  /* start a new warning */
	lua_writestringerror("%s", message);  /* write message */
	if (tocont)  /* not the last part? */
		*warnstate = 2;  /* to be continued */
	else {  /* last part */
		lua_writestringerror("%s", "\n");  /* finish message with end-of-line */
		*warnstate = 1;  /* ready to start a new message */
	}
}

