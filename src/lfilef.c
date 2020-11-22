#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>


/* succ [, errmsg] = system.file (path [, mode [, uperm [, gperm [, operm]]]]) */
static int returnopenfile (lua_State *L) {
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	ssize_t result = (ssize_t)lua_tointeger(L, 2);
	lua_pop(L, 1);
	if (result < 0) return lcuL_pusherrres(L, result);
	*file = result;
	luaL_setmetatable(L, LCU_FILECLS);
	return 1;
}
static void on_fileopen (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	ssize_t result = filereq->result;
	uv_fs_req_cleanup(filereq);
	if (thread) {
		lua_pushinteger(thread, result);
		lcuU_resumereqop(loop, request, 1);
	}
}
static int k_setupfile (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "r");
	int flags = 0;
	int perm = 0;
	int err;
	for (; *mode; mode++) switch (*mode) {
		case 'r': perm |= 1; break;
		case 'w': perm |= 2; break;
		case 'a': perm |= 2; flags |= UV_FS_O_APPEND; break;
		case 's': perm |= 2; flags |= UV_FS_O_SYNC; break;
		case 't': perm |= 2; flags |= UV_FS_O_TRUNC; break;
		case 'n': flags |= UV_FS_O_CREAT; break;
		case 'N': flags |= UV_FS_O_CREAT|UV_FS_O_EXCL; break;
#ifdef O_CLOEXEC
		case 'x': flags |= O_CLOEXEC; break;
#endif
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	switch (perm) {
		case 1: flags |= UV_FS_O_RDONLY; break;
		case 2: flags |= UV_FS_O_WRONLY; break;
		case 3: flags |= UV_FS_O_RDWR; break;
	}
	perm = 0;
	if (flags&UV_FS_O_CREAT) {
		int arg;
		mode = "rw";
		for (arg = 3; arg <= 5; arg++) {
			mode = luaL_optstring(L, arg, mode);
			perm <<= 8;
			for (; *mode; mode++) switch (*mode) {
				case 'r': perm |= 1; break;
				case 'w': perm |= 2; break;
				case 'x': perm |= 4; break;
				default: luaL_error(L, "bad argument #%d, unknown perm char (got '%c')",
				                       arg, *mode);
			}
		}
	}
	lua_settop(L, 0);
	lua_newuserdatauv(L, sizeof(uv_file), 1);  /* raise memory errors */
	lua_pushvalue(L, lua_upvalueindex(1));  /* push scheduler */
	lua_setiuservalue(L, -2, 1);
	err = uv_fs_open(loop, filereq, path, flags, perm, on_fileopen);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_file (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	return lcuT_resetreqopk(L, sched, k_setupfile, returnopenfile, NULL);
}

static uv_file *checkopenfile (lua_State *L, lcu_Scheduler **sched) {
	uv_file *file = (uv_file *)luaL_checkudata(L, 1, LCU_FILECLS);
	luaL_argcheck(L, *file >= 0, 1, "closed file");
	lua_getiuservalue(L, 1, 1);
	*sched = (lcu_Scheduler *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return file;
}

/* ok [, err] = file:close() */
static int file_gc (lua_State *L) {
	lcu_Scheduler *sched;
	uv_file *file = checkopenfile(L, &sched);
	uv_fs_t closereq;
	int err = uv_fs_close(lcu_toloop(sched), &closereq, *file, NULL);
	if (err >= 0) *file = -1;
	return 0;
}

static void on_fileopdone (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	ssize_t result = filereq->result;
	uv_fs_req_cleanup(filereq);
	if (thread) {
		int nret;
		if (result < 0) nret = lcuL_pusherrres(thread, result);
		else {
			lua_pushinteger(thread, result);
			nret = 1;
		}
		lcuU_resumereqop(loop, request, nret);
	}
}

/* bytes [, err] = file:read(buffer [, i [, j [, offset]]]) */
static int k_setupreadfile (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	uv_buf_t bufs[1];  /* args from 2 to 4 (buffer, i, j) */
	int64_t offset = (int64_t)luaL_optinteger(L, 5, -1);
	lcu_getoutputbuf(L, 2, bufs);
	int err = uv_fs_read(loop, filereq, *file, bufs, 1, offset, on_fileopdone);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_read (lua_State *L) {
	lcu_Scheduler *sched;
	checkopenfile(L, &sched);
	return lcuT_resetreqopk(L, sched, k_setupreadfile, NULL, NULL);
}

/* bytes [, err] = file:write(data [, i [, j [, offset]]]) */
static int k_setupwritefile (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	uv_buf_t bufs[1];  /* args from 2 to 4 (data, i, j) */
	int64_t offset = (int64_t)luaL_optinteger(L, 5, -1);
	lcu_getinputbuf(L, 2, bufs);
	int err = uv_fs_write(loop, filereq, *file, bufs, 1, offset, on_fileopdone);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_write (lua_State *L) {
	lcu_Scheduler *sched;
	checkopenfile(L, &sched);
	return lcuT_resetreqopk(L, sched, k_setupwritefile, NULL, NULL);
}


LCUI_FUNC void lcuM_addfilef (lua_State *L) {
	static const luaL_Reg filemt[] = {
		{"__gc", file_gc},
		{"__close", file_gc},
		{NULL, NULL}
	};
	static const luaL_Reg filef[] = {
		{"close", file_gc},
		{"read", file_read},
		{"write", file_write},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"file", system_file},
		{NULL, NULL}
	};

	luaL_newmetatable(L, LCU_FILECLS);
	luaL_setfuncs(L, filemt, 0);
	lua_newtable(L);  /* create method table */
	luaL_setfuncs(L, filef, 0);
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lua_pop(L, 1);  /* pop metatable */

	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
