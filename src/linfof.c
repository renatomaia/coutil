#include "lmodaux.h"

#include <stdlib.h>


typedef struct CpuInfoList {
	int count;
	uv_cpu_info_t *cpu;
} CpuInfoList;

#define checkcpuinfo(L)	((CpuInfoList *)luaL_checkudata(L, 1, LCU_CPUINFOLISTCLS))

static CpuInfoList *openedcpuinfo (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
	luaL_argcheck(L, list->cpu, 1, "closed "LCU_CPUINFOLISTCLS);
	return list;
}

static void closecpuinfo (CpuInfoList *list) {
	if (list->cpu) {
		uv_free_cpu_info(list->cpu, list->count);
		list->cpu = NULL;
		list->count = 0;
	}
}

static int cpuinfo_gc (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
	closecpuinfo(list);
	return 0;
}

/* true = cpuinfo:close() */
static int cpuinfo_close (lua_State *L) {
	CpuInfoList *list = openedcpuinfo(L);
	closecpuinfo(list);
	lua_pushboolean(L, 1);
	return 1;
}

/* cpucount = cpuinfo:count() */
static int cpuinfo_count (lua_State *L) {
	CpuInfoList *list = openedcpuinfo(L);
	lua_pushinteger(L, list->count);
	return 1;
}

/* value = cpuinfo:stats(i, what) */
static int cpuinfo_stats (lua_State *L) {
	CpuInfoList *list = openedcpuinfo(L);
	int i = (int)luaL_checkinteger(L, 2);
	const char *mode = luaL_checkstring(L, 3);
	uv_cpu_info_t *info = list->cpu+i-1;
	luaL_argcheck(L, 0 < i && i <= list->count, 2, "index out of bounds");
	lua_settop(L, 3);
	for (; *mode; mode++) switch (*mode) {
		case 'm': lua_pushstring(L, info->model); break;
		case 'c': lua_pushinteger(L, info->speed); break;
		case 'u': lua_pushinteger(L, info->cpu_times.user); break;
		case 'n': lua_pushinteger(L, info->cpu_times.nice); break;
		case 's': lua_pushinteger(L, info->cpu_times.sys); break;
		case 'i': lua_pushinteger(L, info->cpu_times.idle); break;
		case 'd': lua_pushinteger(L, info->cpu_times.irq); break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	return lua_gettop(L)-3;
}

/* cpuinfo = system.cpuinfo() */
static int system_cpuinfo (lua_State *L) {
	CpuInfoList *list = (CpuInfoList *)lua_newuserdatauv(L, sizeof(CpuInfoList), 0);
	int err = uv_cpu_info(&list->cpu, &list->count);
	if (err) lcu_error(L, err);
	luaL_setmetatable(L, LCU_CPUINFOLISTCLS);
	return 1;
}


/* ... = system.info(what) */

typedef struct SystemInfo {
	int flags;
	double loadavg[3];
	uv_rusage_t rusage;
	uv_passwd_t *passwd;
	uv_utsname_t utsname;
} SystemInfo;

#define LOADAVG	0x01
#define RUSAGE	0x02
#define UTSNAME	0x04
#define PASSWD	0x08

#define SYSUSERINFOCLS	LCU_PREFIX"uv_passwd_t"

#define initsysinf(S) {(S)->flags = 0;}

static int sysuserinfo_gc (lua_State *L) {
	uv_passwd_t *passwd = (uv_passwd_t *)luaL_checkudata(L, 1, SYSUSERINFOCLS);
	uv_os_free_passwd(passwd);
	return 0;
}

static double *sysload (lua_State *L, SystemInfo *sysinf) {
	if (!(sysinf->flags&LOADAVG)) {
		uv_loadavg(sysinf->loadavg);
		sysinf->flags |= LOADAVG;
	}
	return sysinf->loadavg;
}

static uv_rusage_t *sysusage (lua_State *L, SystemInfo *sysinf) {
	if (!(sysinf->flags&RUSAGE)) {
		int err = uv_getrusage(&sysinf->rusage);
		if (err) lcu_error(L, err);
		sysinf->flags |= RUSAGE;
	}
	return &sysinf->rusage;
}

static uv_utsname_t *sysname (lua_State *L, SystemInfo *sysinf) {
	if (!(sysinf->flags&UTSNAME)) {
		int err = uv_os_uname(&sysinf->utsname);
		if (err) lcu_error(L, err);
		sysinf->flags |= UTSNAME;
	}
	return &sysinf->utsname;
}

static uv_passwd_t *sysuser (lua_State *L, SystemInfo *sysinf) {
	if (!(sysinf->flags&PASSWD)) {
		int err;
		sysinf->passwd = (uv_passwd_t *)lua_newuserdatauv(L, sizeof(uv_passwd_t), 0);
		err = uv_os_get_passwd(sysinf->passwd);
		if (err) lcu_error(L, err);
		luaL_setmetatable(L, SYSUSERINFOCLS);
		lua_replace(L, 2);
		sysinf->flags |= PASSWD;
	}
	return sysinf->passwd;
}

typedef int (*GetPathFunc) (char *buffer, size_t *len);

static void pushbuffer(lua_State *L, SystemInfo *sysinf, GetPathFunc f) {
	char array[UV_MAXHOSTNAMESIZE];
	char *buffer = array;
	size_t len = sizeof(array);
	int err = f(buffer, &len);
	if (err == UV_ENOBUFS) {
		buffer = (char *)malloc(len*sizeof(char));
		err = f(buffer, &len);
	}
	if (err >= 0) lua_pushstring(L, buffer);
	if (buffer != array) free(buffer);
	if (err < 0) lcu_error(L, err);
}

static int system_info (lua_State *L) {
	SystemInfo sysinf;
	size_t sz;
	const char *mode = luaL_checklstring(L, 1, &sz);
	initsysinf(&sysinf);
	lua_settop(L, 2);  /* leave space for 'uv_passwd_t' userdata. */
	luaL_argcheck(L, sz < (size_t)INT_MAX, 1, "too many options");
	luaL_checkstack(L, (int)sz, "too many values to return");
	for (; *mode; mode++) switch (*mode) {
		case '#': lua_pushinteger(L, (lua_Integer)uv_os_getpid()); break;
		case '$': lua_pushstring(L, sysuser(L, &sysinf)->shell); break;
		case '^': lua_pushinteger(L, (lua_Integer)uv_os_getppid()); break;
		case '=': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_isrss*1024); break;
		case '<': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_msgrcv); break;
		case '>': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_msgsnd); break;
		case '1': lua_pushnumber(L, (lua_Number)sysload(L, &sysinf)[0]); break;
		case 'b': lua_pushinteger(L, (lua_Integer)uv_get_total_memory()); break;
		case 'c': lua_pushnumber(L, lcu_time2sec(sysusage(L, &sysinf)->ru_utime)); break;
		case 'd': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_idrss*1024); break;
		case 'e': pushbuffer(L, &sysinf, uv_exepath); break;
		case 'f': lua_pushinteger(L, (lua_Integer)uv_get_free_memory()); break;
		case 'g': lua_pushinteger(L, (lua_Integer)sysuser(L, &sysinf)->gid); break;
		case 'h': lua_pushstring(L, sysname(L, &sysinf)->machine); break;
		case 'H': lua_pushstring(L, sysuser(L, &sysinf)->homedir); break;
		case 'i': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_inblock); break;
		case 'k': lua_pushstring(L, sysname(L, &sysinf)->sysname); break;
		case 'l': lua_pushnumber(L, (lua_Number)sysload(L, &sysinf)[1]); break;
		case 'L': lua_pushnumber(L, (lua_Number)sysload(L, &sysinf)[2]); break;
		case 'm': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_ixrss*1024); break;
		case 'M': lua_pushinteger(L, (lua_Integer)uv_get_constrained_memory()); break;
		case 'n': pushbuffer(L, &sysinf, uv_os_gethostname); break;
		case 'o': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_oublock); break;
		case 'P': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_majflt); break;
		case 'p': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_minflt); break;
		case 'r': {
			size_t val;
			uv_resident_set_memory(&val);
			lua_pushinteger(L, (lua_Integer)val);
		} break;
		case 'R': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_maxrss*1024); break;
		case 's': lua_pushnumber(L, lcu_time2sec(sysusage(L, &sysinf)->ru_stime)); break;
		case 'S': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_nsignals); break;
		case 'u': lua_pushinteger(L, (lua_Integer)sysuser(L, &sysinf)->uid); break;
		case 'U': lua_pushstring(L, sysuser(L, &sysinf)->username); break;
		case 't': {
			double val;
			uv_uptime(&val);
			lua_pushnumber(L, (lua_Number)val);
		} break;
		case 'T': pushbuffer(L, &sysinf, uv_os_tmpdir); break;
		case 'v': lua_pushstring(L, sysname(L, &sysinf)->release); break;
		case 'V': lua_pushstring(L, sysname(L, &sysinf)->version); break;
		case 'w': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_nswap); break;
		case 'x': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_nvcsw); break;
		case 'X': lua_pushinteger(L, (lua_Integer)sysusage(L, &sysinf)->ru_nivcsw); break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	return lua_gettop(L)-2;
}


static const luaL_Reg sysuserinfomt[] = {
	{"__gc", sysuserinfo_gc},
	{NULL, NULL}
};
static const luaL_Reg cpuinfomt[] = {
	{"__gc", cpuinfo_gc},
	{"__close", cpuinfo_gc},
	{NULL, NULL}
};
static const luaL_Reg cpuinfof[] = {
	{"close", cpuinfo_close},
	{"count", cpuinfo_count},
	{"stats", cpuinfo_stats},
	{NULL, NULL}
};
static const luaL_Reg modulef[] = {
	{"cpuinfo", system_cpuinfo},
	{"info", system_info},
	{NULL, NULL}
};

LCUI_FUNC void lcuM_addinfof (lua_State *L) {
	luaL_newmetatable(L, SYSUSERINFOCLS);
	luaL_setfuncs(L, sysuserinfomt, 0);
	lua_pop(L, 1);  /* pop metatable */

	luaL_newmetatable(L, LCU_CPUINFOLISTCLS);
	luaL_setfuncs(L, cpuinfomt, 0);
	lua_newtable(L);  /* create method table */
	luaL_setfuncs(L, cpuinfof, 0);
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lua_pop(L, 1);  /* pop metatable */

	luaL_setfuncs(L, modulef, 0);
}
