#include "lmodaux.h"


typedef struct CpuInfoList {
	int count;
	uv_cpu_info_t *cpu;
} CpuInfoList;

#define checkcpuinfo(L)	((CpuInfoList *)luaL_checkudata(L, 1, LCU_CPUINFOLISTCLS))

static uv_cpu_info_t *getcpuinfo (lua_State *L) {
	CpuInfoList *info = checkcpuinfo(L);
	int i = (int)luaL_checkinteger(L, 2);
	luaL_argcheck(L, 0 < i && i <= info->count, 2, "index out of bounds");
	return info->cpu+i-1;
}

static int cpuinfo_gc (lua_State *L) {
	CpuInfoList *info = checkcpuinfo(L);
	uv_free_cpu_info(info->cpu, info->count);
	return 0;
}

/* cpucount = cpuinfo:count() */
static int cpuinfo_count (lua_State *L) {
	CpuInfoList *info = checkcpuinfo(L);
	lua_pushinteger(L, info->count);
	return 1;
}

/* modelname = cpuinfo:model(i) */
static int cpuinfo_model (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushstring(L, info->model);
	return 1;
}

/* speed = cpuinfo:speed(i) */
static int cpuinfo_speed (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->speed);
	return 1;
}

/* msecs = cpuinfo:usertime(i) */
static int cpuinfo_usertime (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->cpu_times.user);
	return 1;
}

/* msecs = cpuinfo:nicetime(i) */
static int cpuinfo_nicetime (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->cpu_times.nice);
	return 1;
}

/* msecs = cpuinfo:systemtime(i) */
static int cpuinfo_systemtime (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->cpu_times.sys);
	return 1;
}

/* msecs = cpuinfo:idletime(i) */
static int cpuinfo_idletime (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->cpu_times.idle);
	return 1;
}

/* msecs = cpuinfo:irqtime(i) */
static int cpuinfo_irqtime (lua_State *L) {
	uv_cpu_info_t *info = getcpuinfo(L);
	lua_pushinteger(L, info->cpu_times.irq);
	return 1;
}

/* cpuinfo = info.getcpustat() */
static int info_getcpustat (lua_State *L) {
	CpuInfoList *list = (CpuInfoList *)lua_newuserdatauv(L, sizeof(CpuInfoList), 0);
	int err = uv_cpu_info(&list->cpu, &list->count);
	if (err) lcu_error(L, err);
	luaL_setmetatable(L, LCU_CPUINFOLISTCLS);
	return 1;
}


#define unmarked(V,F)	(!((V)&(F)) && (V |= (F)))
#define time2secs(T)	((T).tv_sec+((lua_Number)((T).tv_usec)*1e-3))

/* ... = info.getusage(what) */
#define USAGEINFO	0x01
static int info_getusage (lua_State *L) {
	int retrieved = 0;
	uv_rusage_t usage;
	const char *mode = luaL_checkstring(L, 1);
	lua_settop(L, 1);
	for (; *mode; mode++) switch (*mode) {
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
		{"model", cpuinfo_model},
		{"speed", cpuinfo_speed},
		{"usertime", cpuinfo_usertime},
		{"nicetime", cpuinfo_nicetime},
		{"systemtime", cpuinfo_systemtime},
		{"idletime", cpuinfo_idletime},
		{"irqtime", cpuinfo_irqtime},
		{NULL, NULL}
	};
	static const luaL_Reg modulef[] = {
		{"getcpustat", info_getcpustat},
		{"getusage", info_getusage},
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
