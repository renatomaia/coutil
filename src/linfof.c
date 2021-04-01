#include "lmodaux.h"


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

/* getmetatable(cpuinfo).__{gc,close}(cpuinfo) */
static int cpuinfo_gc (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
	if (list->cpu) {
		uv_free_cpu_info(list->cpu, list->count);
		list->cpu = NULL;
		list->count = 0;
	}
	return 0;
}

/* cpucount = getmetatable(cpuinfo).__len(cpuinfo) */
static int cpuinfo_len (lua_State *L) {
	CpuInfoList *list = openedcpuinfo(L);
	lua_pushinteger(L, list->count);
	return 1;
}

/* ... = getmetatable(cpuinfo).__call(cpuinfo, what [, previdx]) */
static int cpuinfo_call (lua_State *L) {
	CpuInfoList *list = openedcpuinfo(L);
	const char *mode = luaL_checkstring(L, 2);
	int i = 1+(int)luaL_optinteger(L, 3, 0);
	uv_cpu_info_t *info = list->cpu+i-1;
	if (0 < i && i <= list->count) {
		lua_settop(L, 2);
		lua_pushinteger(L, i);
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
		return lua_gettop(L)-2;
	}
	return 0;
}

/* cpuinfo = system.cpuinfo(which) */
static int system_cpuinfo (lua_State *L) {
	CpuInfoList *list = (CpuInfoList *)lua_newuserdatauv(L, sizeof(CpuInfoList), 0);
	int err = uv_cpu_info(&list->cpu, &list->count);
	if (err < 0) return lcu_error(L, err);
	luaL_setmetatable(L, LCU_CPUINFOLISTCLS);
	lua_insert(L, 1);  /* cpuinfo below 'which' */
	lua_settop(L, 2);
	lua_pushinteger(L, 0);
	lua_pushvalue(L, 1);  /* cpuinfo */
	return 4;
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
		case 'e': lcu_pushstrout(L, uv_exepath); break;
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
		case 'n': lcu_pushstrout(L, uv_os_gethostname); break;
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
		case 'T': lcu_pushstrout(L, uv_os_tmpdir); break;
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
	{"__len", cpuinfo_len},
	{"__call", cpuinfo_call},
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
	lua_pop(L, 1);  /* pop metatable */

	luaL_setfuncs(L, modulef, 0);
}
