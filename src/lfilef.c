#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>
#ifdef __linux__
#include </usr/include/linux/magic.h>
#endif


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
		mode = luaL_checkstring(L, 3);
		for (; *mode; mode++) switch (*mode) {
			case 'U': perm |= S_ISUID; break;
			case 'G': perm |= S_ISGID; break;
			case 'S': perm |= S_ISVTX; break;
			case 'r': perm |= S_IRUSR; break;
			case 'w': perm |= S_IWUSR; break;
			case 'x': perm |= S_IXUSR; break;
			case 'R': perm |= S_IRGRP; break;
			case 'W': perm |= S_IWGRP; break;
			case 'X': perm |= S_IXGRP; break;
			case '4': perm |= S_IROTH; break;
			case '2': perm |= S_IWOTH; break;
			case '1': perm |= S_IXOTH; break;
			case '_':
			case ' ':
			case '-': break;  /* ignored */
			default: luaL_error(L, "bad argument #3, unknown char (got '%c')", *mode);
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

#define checkfile(L)	((uv_file *)luaL_checkudata(L, 1, LCU_FILECLS))

static lcu_Scheduler *tosched (lua_State *L) {
	lcu_Scheduler *sched;
	lua_getiuservalue(L, 1, 1);
	sched = (lcu_Scheduler *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return sched;
}

static uv_file *openedfile (lua_State *L) {
	uv_file *file = checkfile(L);
	luaL_argcheck(L, *file >= 0, 1, "closed file");
	return file;
}

static int closefile (lua_State *L, uv_file *file) {
	uv_fs_t closereq;
	int err = uv_fs_close(lcu_toloop(tosched(L)), &closereq, *file, NULL);
	if (err >= 0) *file = -1;
	return err;
}

/* ok [, err] = file:close() */
static int file_gc (lua_State *L) {
	uv_file *file = checkfile(L);
	closefile(L, file);
	return 0;
}

/* ok [, err] = file:close() */
static int file_close (lua_State *L) {
	uv_file *file = openedfile(L);
	int err = closefile(L, file);
	return lcuL_pushresults(L, 0, err);
}

static void on_fileopdone (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	ssize_t result = filereq->result;
	uv_fs_req_cleanup(filereq);
	if (thread) {
		int nret;
		if (result < 0) {
			nret = lcuL_pusherrres(thread, result);
		} else {
			lua_pushinteger(thread, result);
			nret = 1;
		}
		lcuU_resumereqop(loop, request, nret);
	}
}

/* bytes [, err] = file:read(buffer [, i [, j [, offset [, mode]]]]) */
static int callread (lua_State *L,
                     uv_loop_t* loop,
                     uv_fs_t* filereq,
                     uv_file file,
                     uv_fs_cb callback) {
	uv_buf_t buf;  /* args from 2 to 4 (data, i, j) */
	int64_t offset = (int64_t)luaL_optinteger(L, 5, -1);
	lcu_getoutputbuf(L, 2, &buf);
	return uv_fs_read(loop, filereq, file, &buf, 1, offset, callback);
}
static int k_setupreadfile (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	int err = callread(L, loop, filereq, *file, on_fileopdone);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_read (lua_State *L) {
	uv_file file = *openedfile(L);
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 6)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		int err = callread(L, loop, &filereq, file, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		return 1;
	}
	return lcuT_resetreqopk(L, sched, k_setupreadfile, NULL, NULL);
}

/* bytes [, err] = file:write(data [, i [, j [, offset [, mode]]]]) */
static int callwrite (lua_State *L,
                      uv_loop_t* loop,
                      uv_fs_t* filereq,
                      uv_file file,
                      uv_fs_cb callback) {
	uv_buf_t buf;  /* args from 2 to 4 (data, i, j) */
	int64_t offset = (int64_t)luaL_optinteger(L, 5, -1);
	lcu_getinputbuf(L, 2, &buf);
	return uv_fs_write(loop, filereq, file, &buf, 1, offset, callback);
}
static int k_setupwritefile (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	int err = callwrite(L, loop, filereq, *file, on_fileopdone);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_write (lua_State *L) {
	uv_file file = *openedfile(L);
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 6)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		int err = callwrite(L, loop, &filereq, file, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		return 1;
	}
	return lcuT_resetreqopk(L, sched, k_setupwritefile, NULL, NULL);
}


/* ... = system.pathinfo (path, which) */

#define PATHINFO_READLINK 0x01  /* uv_fs_readlink */
#define PATHINFO_REALPATH 0x02  /* uv_fs_realpath */
#define PATHINFO_STATFS   0x04  /* uv_fs_statfs */
#define PATHINFO_STAT     0x08  /* uv_fs_stat|uv_fs_lstat */
#define PATHINFO_LSTAT    0x10  /* use 'uv_fs_lstat' */
#define PATHINFO_NOYIELD  0x20
#define PATHINFO_SRCMASK  0x0f
#define PATHINFO_BITMASK  0x3f
#define PATHINFO_BITCOUNT 6

static int checksrc (lua_State *L, const char mode) {
	switch (mode) {
		case '?':
		case 'U':
		case 'G':
		case 'S':
		case 'r':
		case 'w':
		case 'x':
		case 'R':
		case 'W':
		case 'X':
		case '4':
		case '2':
		case '1':
		case 'M':
		case 'd':
		case '#':
		case '*':
		case 'u':
		case 'g':
		case 'D':
		case 'B':
		case 'i':
		case 'b':
		case '_':
		case 'v':
		case 'a':
		case 'm':
		case 's':
		case 'c': return PATHINFO_STAT;
		case '@':
		case 'N':
		case 'I':
		case 'T':
		case 'F':
		case 'A':
		case 't':
		case 'f': return PATHINFO_STATFS;
		case 'p': return PATHINFO_REALPATH;
		case '=': return PATHINFO_READLINK;
		/* options */
		case 'l':
		case LCU_NOYIELDMODE:
			return luaL_error(L, "'%c' must be in the begin of 'mode'", mode);
		default:
			return luaL_error(L, "unknown mode char (got '%c')", mode);
	}
}

static int misssrc (int *bits, int src) {
	if (*bits&src) {
		*bits = (src<<PATHINFO_BITCOUNT)          /* set current source */
		      | ((*bits&~src)&PATHINFO_BITMASK);  /* clear required source */
		return 1;
	}
	return 0;
}

typedef int (*SourceFunc) (uv_loop_t *loop,
                           uv_fs_t *filereq,
                           const char *path,
                           uv_fs_cb callback);

static SourceFunc getsrc (int *bits) {
	if (misssrc(bits, PATHINFO_READLINK))
		return uv_fs_readlink;
	else if (misssrc(bits, PATHINFO_REALPATH))
		return uv_fs_realpath;
	else if (misssrc(bits, PATHINFO_STATFS))
		return uv_fs_statfs;
	else if (misssrc(bits, PATHINFO_STAT))
		return (*bits&PATHINFO_LSTAT) ? uv_fs_lstat : uv_fs_stat;
	lcu_assert(0);
	return NULL;
}

static void pushfiletype (lua_State *L, uint64_t type) {
	switch (type) {
		case S_IFSOCK: lua_pushliteral(L, "socket"); break;
		case S_IFLNK: lua_pushliteral(L, "link"); break;
		case S_IFREG: lua_pushliteral(L, "regular"); break;
		case S_IFBLK: lua_pushliteral(L, "block"); break;
		case S_IFDIR: lua_pushliteral(L, "directory"); break;
		case S_IFCHR: lua_pushliteral(L, "character"); break;
		case S_IFIFO: lua_pushliteral(L, "fifo"); break;
		default: lua_pushliteral(L, "unknown"); break;
	}
}

static void pushfilesystype (lua_State *L, uint64_t type) {
	switch (type) {
#ifdef ADFS_SUPER_MAGIC
		case ADFS_SUPER_MAGIC: lua_pushliteral(L, "adfs"); break;
#endif
#ifdef AFFS_SUPER_MAGIC
		case AFFS_SUPER_MAGIC: lua_pushliteral(L, "affs"); break;
#endif
#ifdef AFS_SUPER_MAGIC
		case AFS_SUPER_MAGIC: lua_pushliteral(L, "afs"); break;
#endif
#ifdef ANON_INODE_FS_MAGIC
		case ANON_INODE_FS_MAGIC: lua_pushliteral(L, "anonymous"); break;
#endif
#ifdef AUTOFS_SUPER_MAGIC
		case AUTOFS_SUPER_MAGIC: lua_pushliteral(L, "autofs"); break;
#endif
#ifdef BDEVFS_MAGIC
		case BDEVFS_MAGIC: lua_pushliteral(L, "bdevfs"); break;
#endif
#ifdef BEFS_SUPER_MAGIC
		case BEFS_SUPER_MAGIC: lua_pushliteral(L, "befs"); break;
#endif
#ifdef BFS_MAGIC
		case BFS_MAGIC: lua_pushliteral(L, "bfs"); break;
#endif
#ifdef BINFMTFS_MAGIC
		case BINFMTFS_MAGIC: lua_pushliteral(L, "binfmtfs"); break;
#endif
#ifdef BPF_FS_MAGIC
		case BPF_FS_MAGIC: lua_pushliteral(L, "bpf"); break;
#endif
#ifdef BTRFS_SUPER_MAGIC
		case BTRFS_SUPER_MAGIC: lua_pushliteral(L, "btrfs"); break;
#endif
#ifdef BTRFS_TEST_MAGIC
		case BTRFS_TEST_MAGIC: lua_pushliteral(L, "btrfs"); break;
#endif
#ifdef CGROUP_SUPER_MAGIC
		case CGROUP_SUPER_MAGIC: lua_pushliteral(L, "cgroup"); break;
#endif
#ifdef CGROUP2_SUPER_MAGIC
		case CGROUP2_SUPER_MAGIC: lua_pushliteral(L, "cgroup2"); break;
#endif
#ifdef CIFS_MAGIC_NUMBER
		case CIFS_MAGIC_NUMBER: lua_pushliteral(L, "cifs"); break;
#endif
#ifdef CODA_SUPER_MAGIC
		case CODA_SUPER_MAGIC: lua_pushliteral(L, "coda"); break;
#endif
#ifdef COH_SUPER_MAGIC
		case COH_SUPER_MAGIC: lua_pushliteral(L, "coh"); break;
#endif
#ifdef CRAMFS_MAGIC
		case CRAMFS_MAGIC: lua_pushliteral(L, "cramfs"); break;
#endif
#ifdef DEBUGFS_MAGIC
		case DEBUGFS_MAGIC: lua_pushliteral(L, "debugfs"); break;
#endif
#ifdef DEVFS_SUPER_MAGIC
		case DEVFS_SUPER_MAGIC: lua_pushliteral(L, "devfs"); break;
#endif
#ifdef DEVPTS_SUPER_MAGIC
		case DEVPTS_SUPER_MAGIC: lua_pushliteral(L, "devpts"); break;
#endif
#ifdef ECRYPTFS_SUPER_MAGIC
		case ECRYPTFS_SUPER_MAGIC: lua_pushliteral(L, "ecryptfs"); break;
#endif
#ifdef EFIVARFS_MAGIC
		case EFIVARFS_MAGIC: lua_pushliteral(L, "efivarfs"); break;
#endif
#ifdef EFS_SUPER_MAGIC
		case EFS_SUPER_MAGIC: lua_pushliteral(L, "efs"); break;
#endif
#ifdef EXT4_SUPER_MAGIC
		case EXT4_SUPER_MAGIC: lua_pushliteral(L, "ext4"); break;
#elif EXT3_SUPER_MAGIC
		case EXT3_SUPER_MAGIC: lua_pushliteral(L, "ext3"); break;
#elif EXT2_SUPER_MAGIC
		case EXT2_SUPER_MAGIC: lua_pushliteral(L, "ext2"); break;
#endif
#ifdef EXT2_OLD_SUPER_MAGIC
		case EXT2_OLD_SUPER_MAGIC: lua_pushliteral(L, "ext2_old"); break;
#endif
#ifdef EXT_SUPER_MAGIC
		case EXT_SUPER_MAGIC: lua_pushliteral(L, "ext"); break;
#endif
#ifdef F2FS_SUPER_MAGIC
		case F2FS_SUPER_MAGIC: lua_pushliteral(L, "f2fs"); break;
#endif
#ifdef FUSE_SUPER_MAGIC
		case FUSE_SUPER_MAGIC: lua_pushliteral(L, "fuse"); break;
#endif
#ifdef HFS_SUPER_MAGIC
		case HFS_SUPER_MAGIC: lua_pushliteral(L, "hfs"); break;
#endif
#ifdef HOSTFS_SUPER_MAGIC
		case HOSTFS_SUPER_MAGIC: lua_pushliteral(L, "hostfs"); break;
#endif
#ifdef HPFS_SUPER_MAGIC
		case HPFS_SUPER_MAGIC: lua_pushliteral(L, "hpfs"); break;
#endif
#ifdef HUGETLBFS_MAGIC
		case HUGETLBFS_MAGIC: lua_pushliteral(L, "hugetlbfs"); break;
#endif
#ifdef ISOFS_SUPER_MAGIC
		case ISOFS_SUPER_MAGIC: lua_pushliteral(L, "isofs"); break;
#endif
#ifdef JFFS2_SUPER_MAGIC
		case JFFS2_SUPER_MAGIC: lua_pushliteral(L, "jffs2"); break;
#endif
#ifdef JFS_SUPER_MAGIC
		case JFS_SUPER_MAGIC: lua_pushliteral(L, "jfs"); break;
#endif
#ifdef MINIX_SUPER_MAGIC
		case MINIX_SUPER_MAGIC: lua_pushliteral(L, "minix"); break;
#endif
#ifdef MINIX_SUPER_MAGIC2
		case MINIX_SUPER_MAGIC2: lua_pushliteral(L, "minix+"); break;
#endif
#ifdef MINIX2_SUPER_MAGIC
		case MINIX2_SUPER_MAGIC: lua_pushliteral(L, "minix2"); break;
#endif
#ifdef MINIX2_SUPER_MAGIC2
		case MINIX2_SUPER_MAGIC2: lua_pushliteral(L, "minix2+"); break;
#endif
#ifdef MINIX3_SUPER_MAGIC
		case MINIX3_SUPER_MAGIC: lua_pushliteral(L, "minix3"); break;
#endif
#ifdef MQUEUE_MAGIC
		case MQUEUE_MAGIC: lua_pushliteral(L, "mqueue"); break;
#endif
#ifdef MSDOS_SUPER_MAGIC
		case MSDOS_SUPER_MAGIC: lua_pushliteral(L, "msdos"); break;
#endif
#ifdef MTD_INODE_FS_MAGIC
		case MTD_INODE_FS_MAGIC: lua_pushliteral(L, "mtd"); break;
#endif
#ifdef NCP_SUPER_MAGIC
		case NCP_SUPER_MAGIC: lua_pushliteral(L, "ncp"); break;
#endif
#ifdef NFS_SUPER_MAGIC
		case NFS_SUPER_MAGIC: lua_pushliteral(L, "nfs"); break;
#endif
#ifdef NILFS_SUPER_MAGIC
		case NILFS_SUPER_MAGIC: lua_pushliteral(L, "nilfs"); break;
#endif
#ifdef NSFS_MAGIC
		case NSFS_MAGIC: lua_pushliteral(L, "nsfs"); break;
#endif
#ifdef NTFS_SB_MAGIC
		case NTFS_SB_MAGIC: lua_pushliteral(L, "ntfs_sb"); break;
#endif
#ifdef OCFS2_SUPER_MAGIC
		case OCFS2_SUPER_MAGIC: lua_pushliteral(L, "ocfs2"); break;
#endif
#ifdef OPENPROM_SUPER_MAGIC
		case OPENPROM_SUPER_MAGIC: lua_pushliteral(L, "openprom"); break;
#endif
#ifdef OVERLAYFS_SUPER_MAGIC
		case OVERLAYFS_SUPER_MAGIC: lua_pushliteral(L, "overlayfs"); break;
#endif
#ifdef PIPEFS_MAGIC
		case PIPEFS_MAGIC: lua_pushliteral(L, "pipefs"); break;
#endif
#ifdef PROC_SUPER_MAGIC
		case PROC_SUPER_MAGIC: lua_pushliteral(L, "proc"); break;
#endif
#ifdef PSTOREFS_MAGIC
		case PSTOREFS_MAGIC: lua_pushliteral(L, "pstorefs"); break;
#endif
#ifdef QNX4_SUPER_MAGIC
		case QNX4_SUPER_MAGIC: lua_pushliteral(L, "qnx4"); break;
#endif
#ifdef QNX6_SUPER_MAGIC
		case QNX6_SUPER_MAGIC: lua_pushliteral(L, "qnx6"); break;
#endif
#ifdef RAMFS_MAGIC
		case RAMFS_MAGIC: lua_pushliteral(L, "ramfs"); break;
#endif
#ifdef REISERFS_SUPER_MAGIC
		case REISERFS_SUPER_MAGIC: lua_pushliteral(L, "reiserfs"); break;
#endif
#ifdef ROMFS_MAGIC
		case ROMFS_MAGIC: lua_pushliteral(L, "romfs"); break;
#endif
#ifdef SECURITYFS_MAGIC
		case SECURITYFS_MAGIC: lua_pushliteral(L, "securityfs"); break;
#endif
#ifdef SELINUX_MAGIC
		case SELINUX_MAGIC: lua_pushliteral(L, "selinux"); break;
#endif
#ifdef SMACK_MAGIC
		case SMACK_MAGIC: lua_pushliteral(L, "smack"); break;
#endif
#ifdef SMB2_MAGIC_NUMBER
		case SMB2_MAGIC_NUMBER: lua_pushliteral(L, "smb2"); break;
#endif
#ifdef SMB_SUPER_MAGIC
		case SMB_SUPER_MAGIC: lua_pushliteral(L, "smb"); break;
#endif
#ifdef SOCKFS_MAGIC
		case SOCKFS_MAGIC: lua_pushliteral(L, "sockfs"); break;
#endif
#ifdef SQUASHFS_MAGIC
		case SQUASHFS_MAGIC: lua_pushliteral(L, "squashfs"); break;
#endif
#ifdef SYSFS_MAGIC
		case SYSFS_MAGIC: lua_pushliteral(L, "sysfs"); break;
#endif
#ifdef SYSV2_SUPER_MAGIC
		case SYSV2_SUPER_MAGIC: lua_pushliteral(L, "sysv2"); break;
#endif
#ifdef SYSV4_SUPER_MAGIC
		case SYSV4_SUPER_MAGIC: lua_pushliteral(L, "sysv4"); break;
#endif
#ifdef TMPFS_MAGIC
		case TMPFS_MAGIC: lua_pushliteral(L, "tmpfs"); break;
#endif
#ifdef TRACEFS_MAGIC
		case TRACEFS_MAGIC: lua_pushliteral(L, "tracefs"); break;
#endif
#ifdef UDF_SUPER_MAGIC
		case UDF_SUPER_MAGIC: lua_pushliteral(L, "udf"); break;
#endif
#ifdef UFS_MAGIC
		case UFS_MAGIC: lua_pushliteral(L, "ufs"); break;
#endif
#ifdef USBDEVICE_SUPER_MAGIC
		case USBDEVICE_SUPER_MAGIC: lua_pushliteral(L, "usbdevice"); break;
#endif
#ifdef V9FS_MAGIC
		case V9FS_MAGIC: lua_pushliteral(L, "v9fs"); break;
#endif
#ifdef VXFS_SUPER_MAGIC
		case VXFS_SUPER_MAGIC: lua_pushliteral(L, "vxfs"); break;
#endif
#ifdef XENFS_SUPER_MAGIC
		case XENFS_SUPER_MAGIC: lua_pushliteral(L, "xenfs"); break;
#endif
#ifdef XENIX_SUPER_MAGIC
		case XENIX_SUPER_MAGIC: lua_pushliteral(L, "xenix"); break;
#endif
#ifdef XFS_SUPER_MAGIC
		case XFS_SUPER_MAGIC: lua_pushliteral(L, "xfs"); break;
#endif
#ifdef XIAFS_SUPER_MAGIC
		case XIAFS_SUPER_MAGIC: lua_pushliteral(L, "xiafs"); break;#endif
#endif
		default: lua_pushliteral(L, "unknown"); break;
	}
}

static void placeat (lua_State *L, int idx) {
	int top = lua_gettop(L);
	if (idx < top) {
		lua_replace(L, idx);
	} else if (idx > top) {
		lua_settop(L, idx-1);
		lua_pushvalue(L, top);
	}
}

static void pushpathinfo (lua_State *L,
	                        int base,
	                        int bits,
	                        const char *mode,
	                        uv_fs_t *filereq) {
	char selected = 'p';
	int i;
	switch (bits>>PATHINFO_BITCOUNT) {  /* get current source */
		case PATHINFO_READLINK: selected = '=';
		case PATHINFO_REALPATH: {
			for (i = 0; mode[i]; i++) if (mode[i] == selected) {
				lua_pushstring(L, (const char *)filereq->ptr);
				placeat(L, i+1+base);
			}
		} break;
		case PATHINFO_STATFS: {
			uv_statfs_t *statfs = (uv_statfs_t *)filereq->ptr;
			for (i = 0; mode[i]; i++) {
				switch (mode[i]) {
					case '@': pushfilesystype(L, statfs->f_type); break;
					case 'N': lua_pushinteger(L, (lua_Integer)statfs->f_type); break;
					case 'I': lua_pushinteger(L, (lua_Integer)statfs->f_bsize); break;
					case 'T': lua_pushinteger(L, (lua_Integer)statfs->f_blocks); break;
					case 'F': lua_pushinteger(L, (lua_Integer)statfs->f_bfree); break;
					case 'A': lua_pushinteger(L, (lua_Integer)statfs->f_bavail); break;
					case 't': lua_pushinteger(L, (lua_Integer)statfs->f_files); break;
					case 'f': lua_pushinteger(L, (lua_Integer)statfs->f_ffree); break;
					default: continue;
				}
				placeat(L, i+1+base);
			}
		} break;
		case PATHINFO_STAT: {
			for (i = 0; mode[i]; i++) {
				switch (mode[i]) {
					case '?': pushfiletype(L, filereq->statbuf.st_mode&S_IFMT); break;
					case 'U': lua_pushboolean(L, filereq->statbuf.st_mode&S_ISUID); break;
					case 'G': lua_pushboolean(L, filereq->statbuf.st_mode&S_ISGID); break;
					case 'S': lua_pushboolean(L, filereq->statbuf.st_mode&S_ISVTX); break;
					case 'r': lua_pushboolean(L, filereq->statbuf.st_mode&S_IRUSR); break;
					case 'w': lua_pushboolean(L, filereq->statbuf.st_mode&S_IWUSR); break;
					case 'x': lua_pushboolean(L, filereq->statbuf.st_mode&S_IXUSR); break;
					case 'R': lua_pushboolean(L, filereq->statbuf.st_mode&S_IRGRP); break;
					case 'W': lua_pushboolean(L, filereq->statbuf.st_mode&S_IWGRP); break;
					case 'X': lua_pushboolean(L, filereq->statbuf.st_mode&S_IXGRP); break;
					case '4': lua_pushboolean(L, filereq->statbuf.st_mode&S_IROTH); break;
					case '2': lua_pushboolean(L, filereq->statbuf.st_mode&S_IWOTH); break;
					case '1': lua_pushboolean(L, filereq->statbuf.st_mode&S_IXOTH); break;
					case 'M': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_mode); break;
					case 'd': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_dev); break;
					case '#': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_ino); break;
					case '*': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_nlink); break;
					case 'u': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_uid); break;
					case 'g': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_gid); break;
					case 'D': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_rdev); break;
					case 'B': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_size); break;
					case 'i': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_blksize); break;
					case 'b': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_blocks); break;
					case '_': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_flags); break;
					case 'v': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_gen); break;
					case 'a': lua_pushnumber(L, lcu_ntime2sec(filereq->statbuf.st_atim)); break;
					case 'm': lua_pushnumber(L, lcu_ntime2sec(filereq->statbuf.st_mtim)); break;
					case 's': lua_pushnumber(L, lcu_ntime2sec(filereq->statbuf.st_ctim)); break;
					case 'c': lua_pushnumber(L, lcu_ntime2sec(filereq->statbuf.st_birthtim)); break;
					default: continue;
				}
				placeat(L, i+1+base);
			}
		} break;
	}
}

static int yieldpathinfo (lua_State *L);

static int returnpathinfo (lua_State *L) {
	uv_fs_t *filereq = (uv_fs_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if (filereq->result >= 0) {
		int bits = lua_tointeger(L, 3);
		const char *mode = (const char *)lua_touserdata(L, 4);
		pushpathinfo(L, 4, bits, mode, filereq);
		uv_fs_req_cleanup(filereq);
		return yieldpathinfo(L);
	} else {
		ssize_t result = filereq->result;
		uv_fs_req_cleanup(filereq);
		return lcuL_pusherrres(L, result);
	}
}
static void on_pathinfo (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		lua_pushlightuserdata(thread, filereq);
		lcuU_resumereqop(loop, request, 1);
	}
	else uv_fs_req_cleanup(filereq);
}

static int k_setuppathinfo (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = lua_tostring(L, 1);
	int bits = lua_tointeger(L, 3);
	int err = getsrc(&bits)(loop, filereq, path, on_pathinfo);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_pushinteger(L, bits);
	lua_replace(L, 3);
	return -1;  /* yield on success */
}

static int yieldpathinfo (lua_State *L) {
	int bits = lua_tointeger(L, 3);
	if (bits&PATHINFO_SRCMASK) {
		lcu_Scheduler *sched = lcu_getsched(L);
		return lcuT_resetreqopk(L, sched, k_setuppathinfo, returnpathinfo, NULL);
	}
	return lua_gettop(L)-4;
}

static int system_pathinfo (lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	const char *mode = luaL_checkstring(L, 2);
	int i, bits = 0;
	for (; *mode; mode++) {
		switch (*mode) {
			case 'l': i = PATHINFO_LSTAT; break;
			case LCU_NOYIELDMODE: i = PATHINFO_NOYIELD; break;
			default: i = 0;
		}
		if (i == 0) break;
		bits |= i;
	}
	for (i = 0; mode[i]; i++) bits |= checksrc(L, mode[i]);
	lua_settop(L, 2);
	luaL_checkstack(L, i+5, "too many values");  /* 5 extra slots */
	if (bits&PATHINFO_NOYIELD) {
		lcu_Scheduler *sched = lcu_getsched(L);
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		while (bits&PATHINFO_SRCMASK) {
			int err = getsrc(&bits)(loop, &filereq, path, NULL);
			if (err < 0) return lcuL_pusherrres(L, err);
			pushpathinfo(L, 2, bits, mode, &filereq);
			uv_fs_req_cleanup(&filereq);
		}
		return i;
	}
	lua_pushinteger(L, bits);
	lua_pushlightuserdata(L, (void *)mode);
	return yieldpathinfo(L);
}


LCUI_FUNC void lcuM_addfilef (lua_State *L) {
	static const luaL_Reg filemt[] = {
		{"__gc", file_gc},
		{"__close", file_gc},
		{NULL, NULL}
	};
	static const luaL_Reg filef[] = {
		{"close", file_close},
		{"read", file_read},
		{"write", file_write},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"file", system_file},
		{"pathinfo", system_pathinfo},
		{NULL, NULL}
	};
	static const struct { const char *name; int value; } bits[] = {
		{ "type", S_IFMT },
		{ "socket", S_IFSOCK },
		{ "link", S_IFLNK },
		{ "regular", S_IFREG },
		{ "block", S_IFBLK },
		{ "directory", S_IFDIR },
		{ "character", S_IFCHR },
		{ "fifo", S_IFIFO },
		{ "setuid", S_ISUID },
		{ "setgid", S_ISGID },
		{ "sticky", S_ISVTX },
		{ "ruser", S_IRUSR },
		{ "wuser", S_IWUSR },
		{ "xuser", S_IXUSR },
		{ "rgroup", S_IRGRP },
		{ "wgroup", S_IWGRP },
		{ "xgroup", S_IXGRP },
		{ "rother", S_IROTH },
		{ "wother", S_IWOTH },
		{ "xother", S_IXOTH },
		{ NULL, 0 }
	};
	int i;

	luaL_newmetatable(L, LCU_FILECLS);
	luaL_setfuncs(L, filemt, 0);
	lua_newtable(L);  /* create method table */
	luaL_setfuncs(L, filef, 0);
	lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
	lua_pop(L, 1);  /* pop metatable */

	lua_pushliteral(L, "filemode");
	lua_createtable(L, 0, 12);
	for (i = 0; bits[i].name; i++) {
		lua_pushstring(L, bits[i].name);
		lua_pushinteger(L, bits[i].value);
		lua_settable(L, -3);
	};
	lua_settable(L, -3);

	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
