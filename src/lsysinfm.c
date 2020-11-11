#include "lmodaux.h"


typedef struct CpuInfoList {
	int count;
	uv_cpu_info_t *cpu;
} CpuInfoList;

#define checkcpuinfo(L)	((CpuInfoList *)luaL_checkudata(L, 1, LCU_CPUINFOLISTCLS))

static int cpuinfo_gc (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
	uv_free_cpu_info(list->cpu, list->count);
	return 0;
}

/* cpucount = cpuinfo:count() */
static int cpuinfo_count (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
	lua_pushinteger(L, list->count);
	return 1;
}

/* value = cpuinfo:stats(i, what) */
static int cpuinfo_stats (lua_State *L) {
	CpuInfoList *list = checkcpuinfo(L);
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

/* cpuinfo = info.getcpustats() */
static int info_getcpustats (lua_State *L) {
	CpuInfoList *list = (CpuInfoList *)lua_newuserdatauv(L, sizeof(CpuInfoList), 0);
	int err = uv_cpu_info(&list->cpu, &list->count);
	if (err) lcu_error(L, err);
	luaL_setmetatable(L, LCU_CPUINFOLISTCLS);
	return 1;
}


#define unmarked(V,F)	(!((V)&(F)) && (V |= (F)))
#define time2secs(T)	((T).tv_sec+((lua_Number)((T).tv_usec)*1e-3))

/* ... = info.getprocess(what) */
#define USAGEINFO	0x01
static int info_getprocess (lua_State *L) {
	int retrieved = 0;
	uv_rusage_t usage;
	const char *mode = luaL_checkstring(L, 1);
	lua_settop(L, 1);
	for (; *mode; mode++) switch (*mode) {
		case '#': lua_pushinteger(L, (lua_Integer)uv_os_getpid()); break;
		case '^': lua_pushinteger(L, (lua_Integer)uv_os_getppid()); break;
		case 'c': lua_pushinteger(L, (lua_Integer)uv_get_constrained_memory()); break;
		case 'm': {
			size_t rssbytes;
			uv_resident_set_memory(&rssbytes);
			lua_pushinteger(L, rssbytes);
		} break;
		default: {
			if (unmarked(retrieved, USAGEINFO)) uv_getrusage(&usage);
			switch (*mode) {
				case 'U': lua_pushnumber(L, time2secs(usage.ru_utime)); break;
				case 'S': lua_pushnumber(L, time2secs(usage.ru_stime)); break;
				case 'T': lua_pushinteger(L, (lua_Integer)(usage.ru_maxrss)*1000); break;
				case 'M': lua_pushinteger(L, (lua_Integer)(usage.ru_ixrss)*1000); break;
				case 'd': lua_pushinteger(L, (lua_Integer)(usage.ru_idrss)*1000); break;
				case '=': lua_pushinteger(L, (lua_Integer)(usage.ru_isrss)*1000); break;
				case 'p': lua_pushinteger(L, (lua_Integer)usage.ru_minflt); break;
				case 'P': lua_pushinteger(L, (lua_Integer)usage.ru_majflt); break;
				case 'w': lua_pushinteger(L, (lua_Integer)usage.ru_nswap); break;
				case 'i': lua_pushinteger(L, (lua_Integer)usage.ru_inblock); break;
				case 'o': lua_pushinteger(L, (lua_Integer)usage.ru_oublock); break;
				case '>': lua_pushinteger(L, (lua_Integer)usage.ru_msgsnd); break;
				case '<': lua_pushinteger(L, (lua_Integer)usage.ru_msgrcv); break;
				case 's': lua_pushinteger(L, (lua_Integer)usage.ru_nsignals); break;
				case 'x': lua_pushinteger(L, (lua_Integer)usage.ru_nvcsw); break;
				case 'X': lua_pushinteger(L, (lua_Integer)usage.ru_nivcsw); break;
				default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
			}
		} break;
	}
	return lua_gettop(L)-1;
}

/* ... = info.getsystem(what) */
#define LOADAVERAGE	0x01
static int info_getsystem (lua_State *L) {
	int retrieved = 0;
	double loadavg[3];
	const char *mode = luaL_checkstring(L, 1);
	lua_settop(L, 1);
	for (; *mode; mode++) switch (*mode) {
		case 'f': lua_pushinteger(L, (lua_Integer)uv_get_free_memory()); break;
		case 'p': lua_pushinteger(L, (lua_Integer)uv_get_total_memory()); break;
		case 'u': {
			double uptime;
			uv_uptime(&uptime);
			lua_pushnumber(L, (lua_Number)uptime);
		} break;
		default: {
			if (unmarked(retrieved, LOADAVERAGE)) uv_loadavg(loadavg);
			switch (*mode) {
				case '1': lua_pushnumber(L, (lua_Number)loadavg[0]); break;
				case 'l': lua_pushnumber(L, (lua_Number)loadavg[1]); break;
				case 'L': lua_pushnumber(L, (lua_Number)loadavg[2]); break;
				default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
			}
		} break;
	}
	return lua_gettop(L)-1;
}


LCUMOD_API int luaopen_coutil_info (lua_State *L) {
	static const luaL_Reg cpuinfomt[] = {
		{"__gc", cpuinfo_gc},
		{NULL, NULL}
	};
	static const luaL_Reg cpuinfof[] = {
		{"count", cpuinfo_count},
		{"stats", cpuinfo_stats},
		{NULL, NULL}
	};
	static const luaL_Reg modulef[] = {
		{"getcpustats", info_getcpustats},
		{"getprocess", info_getprocess},
		{"getsystem", info_getsystem},
		{NULL, NULL}
	};

	luaL_newmetatable(L, LCU_CPUINFOLISTCLS);
	luaL_setfuncs(L, cpuinfomt, 0);
	lua_newtable(L);  /* create method table */
	luaL_setfuncs(L, cpuinfof, 0);
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lua_pop(L, 1);  /* pop metatable */

	luaL_newlib(L, modulef);
	return 1;
}
