#include "lmodaux.h"
#include "loperaux.h"

#include <lmemlib.h>
#ifdef __linux__
#include </usr/include/linux/magic.h>
#endif


static int checkperm (lua_State *L, int arg) {
	const char *mode = lua_tostring(L, arg);
	if (mode) {
		int perm = 0;
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
			default: luaL_error(L,
				"bad argument #%d, unknown perm char (got '%c')", arg, *mode);
		}
		return perm;
	}
	return (int)luaL_checkinteger(L, arg);
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

static int pushfilestat (lua_State *L, char mode, uv_fs_t *filereq) {
	switch (mode) {
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
		default: return 0;
	}
	return 1;
}


/*
 * File Info
 */

#define INFO_READLINK 0x01  /* uv_fs_readlink */
#define INFO_REALPATH 0x02  /* uv_fs_realpath */
#define INFO_STATFS   0x04  /* uv_fs_statfs */
#define INFO_STAT     0x08  /* uv_fs_stat|uv_fs_lstat */
#define INFO_LSTAT    0x10  /* use 'uv_fs_lstat' instead */
#define INFO_NOYIELD  0x20
#define INFO_SRCMASK  0x0f
#define INFO_BITMASK  0x3f
#define INFO_BITCOUNT 6

static void placeat (lua_State *L, int idx) {
	int top = lua_gettop(L);
	if (idx < top) {
		lua_replace(L, idx);
	} else if (idx > top) {
		lua_settop(L, idx-1);
		lua_pushvalue(L, top);
	}
}

static void pushfileinfo (lua_State *L,
	                        int base,
	                        int bits,
	                        const char *mode,
	                        uv_fs_t *filereq) {
	char selected = 'p';
	int i;
	switch (bits>>INFO_BITCOUNT) {  /* get current source */
		case INFO_READLINK: selected = '=';
		case INFO_REALPATH: {
			for (i = 0; mode[i]; i++) if (mode[i] == selected) {
				if (filereq->result == UV_EINVAL) lua_pushnil(L);
				else lua_pushstring(L, (const char *)filereq->ptr);
				placeat(L, i+1+base);
			}
		} break;
		case INFO_STATFS: {
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
		case INFO_STAT: {
			for (i = 0; mode[i]; i++)
				if (pushfilestat(L, mode[i], filereq))
					placeat(L, i+1+base);
		} break;
	}
}

static int toinfosrc (lua_State *L, const char mode, int mask) {
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
		case 'c': return INFO_STAT;
		case '@':
		case 'N':
		case 'I':
		case 'T':
		case 'F':
		case 'A':
		case 't':
		case 'f': return INFO_STATFS;
		case 'p': return INFO_REALPATH;
		case '=': return INFO_READLINK;
		/* options */
		case 'l': if (!(mask&INFO_LSTAT)) break;
		case LCU_NOYIELDMODE: if (!(mask&INFO_NOYIELD)) break;
			return luaL_error(L, "'%c' must be in the begin of 'mode'", mode);
	}
	return 0;
}

static const char *checkinfomode (lua_State *L, int arg, int *bits) {
	const char *mode = luaL_checkstring(L, arg);
	int i, mask = *bits;
	*bits = 0;
	for (; *mode; mode++) {
		switch (*mode) {
			case 'l': i = INFO_LSTAT; break;
			case LCU_NOYIELDMODE: i = INFO_NOYIELD; break;
			default: i = 0;
		}
		if (i&mask) *bits |= i;
		else if (i == 0) break;
	}
	for (i = 0; mode[i]; i++) {
		int sel = toinfosrc(L, mode[i], mask);
		if (sel&mask) *bits |= sel;
		else luaL_error(L, "unknown mode char (got '%c')", mode[i]);
	}
	lua_settop(L, arg);
	luaL_checkstack(L, i+5, "too many values");  /* 5 extra slots */
	return mode;
}

static int missinfosrc (int *bits, int src) {
	if (*bits&src) {
		*bits = (src<<INFO_BITCOUNT)          /* set current source */
		      | ((*bits&~src)&INFO_BITMASK);  /* clear required source */
		return 1;
	}
	return 0;
}

static void getinfosrc (lua_State *L,
                        int *bits,
                        uv_loop_t *loop,
                        uv_fs_t *filereq,
                        const char *path,
                        uv_fs_cb callback) {
	int err;
	if (missinfosrc(bits, INFO_STAT)) {
		err = (*bits&INFO_LSTAT) ? uv_fs_lstat(loop, filereq, path, callback)
		                         : uv_fs_stat(loop, filereq, path, callback);
	} else if (missinfosrc(bits, INFO_STATFS)) {
		err = uv_fs_statfs(loop, filereq, path, callback);
	} else if (missinfosrc(bits, INFO_REALPATH)) {
		err = uv_fs_realpath(loop, filereq, path, callback);
	} else if (missinfosrc(bits, INFO_READLINK)) {
		err = uv_fs_readlink(loop, filereq, path, callback);
		if (err == UV_EINVAL) {
			filereq->result = UV_EINVAL;
			err = 0;
		}
	}
	if (err < 0) lcu_error(L, err);
}

/* ... = system.fileinfo (path, which) */
static int yieldfileinfo (lua_State *L);
static int returnfileinfo (lua_State *L) {
	int bits = lua_tointeger(L, 3);
	uv_fs_t *filereq = (uv_fs_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if ( filereq->result >= 0 ||
	     (bits&(INFO_READLINK<<INFO_BITCOUNT) && filereq->result == UV_EINVAL) ) {
		const char *mode = (const char *)lua_touserdata(L, 4);
		pushfileinfo(L, 4, bits, mode, filereq);
		uv_fs_req_cleanup(filereq);
		return yieldfileinfo(L);
	} else {
		ssize_t result = filereq->result;
		uv_fs_req_cleanup(filereq);
		return lcu_error(L, result);
	}
}
static void on_fileinfo (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endreqop(loop, request);
	if (thread) {
		lua_pushlightuserdata(thread, filereq);
		lcuU_resumereqop(loop, request, 1);
	}
	else uv_fs_req_cleanup(filereq);
}
static int k_setupfileinfo (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = lua_tostring(L, 1);
	int bits = lua_tointeger(L, 3);
	getinfosrc(L, &bits, loop, filereq, path, on_fileinfo);
	lua_pushinteger(L, bits);
	lua_replace(L, 3);
	return -1;  /* yield on success */
}
static int yieldfileinfo (lua_State *L) {
	int bits = lua_tointeger(L, 3);
	if (bits&INFO_SRCMASK) {
		lcu_Scheduler *sched = lcu_getsched(L);
		return lcuT_resetreqopk(L, sched, k_setupfileinfo, returnfileinfo, NULL);
	}
	return lua_gettop(L)-4;
}
static int system_fileinfo (lua_State *L) {
	int bits = INFO_BITMASK;  /* accept all options */
	const char *path = luaL_checkstring(L, 1);
	const char *mode = checkinfomode(L, 2, &bits);
	if (bits&INFO_NOYIELD) {
		lcu_Scheduler *sched = lcu_getsched(L);
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		while (bits&INFO_SRCMASK) {
			getinfosrc(L, &bits, loop, &filereq, path, NULL);
			pushfileinfo(L, 2, bits, mode, &filereq);
			uv_fs_req_cleanup(&filereq);
		}
		return lua_gettop(L)-2;
	}
	lua_pushinteger(L, bits);
	lua_pushlightuserdata(L, (void *)mode);
	return yieldfileinfo(L);
}


/*
 * Directories
 */

typedef struct DirectoryList {
	lua_CFunction results;
	lua_CFunction cancel;
	uv_fs_t filereq;
} DirectoryList;

#define checkdirinfo(L)	((DirectoryList *)luaL_checkudata(L, 1, LCU_DIRECTORYLISTCLS))

static DirectoryList *openeddirinfo (lua_State *L) {
	DirectoryList *dirlist = checkdirinfo(L);
	luaL_argcheck(L, dirlist->filereq.type != UV_UNKNOWN_REQ, 1, "closed");
	return dirlist;
}

/* getmetatable(dirinfo).__{gc,close}(dirinfo) */
static int dirinfo_gc (lua_State *L) {
	DirectoryList *dirlist = checkdirinfo(L);
	if (dirlist->filereq.type != UV_UNKNOWN_REQ) {
		uv_fs_req_cleanup(&dirlist->filereq);
		dirlist->filereq.type = UV_UNKNOWN_REQ;
	}
	return 0;
}

/* ... = getmetatable(dirinfo).__call(dirinfo) */
static int dirinfo_call (lua_State *L) {
	DirectoryList *dirlist = openeddirinfo(L);
	uv_dirent_t entry;
	int err = uv_fs_scandir_next(&dirlist->filereq, &entry);
	if (err < 0) {
		uv_fs_req_cleanup(&dirlist->filereq);
		dirlist->filereq.type = UV_UNKNOWN_REQ;
		return 0;
	}
	lua_pushstring(L, entry.name);
	switch (entry.type) {
		case UV_DIRENT_SOCKET: lua_pushliteral(L, "socket"); break;
		case UV_DIRENT_LINK: lua_pushliteral(L, "link"); break;
		case UV_DIRENT_FILE: lua_pushliteral(L, "regular"); break;
		case UV_DIRENT_BLOCK: lua_pushliteral(L, "block"); break;
		case UV_DIRENT_DIR: lua_pushliteral(L, "directory"); break;
		case UV_DIRENT_CHAR: lua_pushliteral(L, "character"); break;
		case UV_DIRENT_FIFO: lua_pushliteral(L, "fifo"); break;
		default: lua_pushliteral(L, "unknown"); break;
	}
	return 2;
}

/* iterator, state, varinit, closing = system.listdir (path [, mode]) */
static int returnlistdir (lua_State *L) {
	return 1;
}
static void uv_onlistdir (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endobjreqop(loop, request);
	if (thread) lcuU_resumeobjreqop(loop, request, 0);
	else {
		uv_fs_req_cleanup(filereq);
		filereq->type = UV_UNKNOWN_REQ;
	}
}
static int k_setuplistdir (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = lua_tostring(L, 1);
	int err = uv_fs_scandir(loop, filereq, path, 0, uv_onlistdir);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_listdir (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *path = luaL_checkstring(L, 1);
	int noyield = lcuL_checknoyieldmode(L, 2);
	DirectoryList *dirlist;
	lua_settop(L, 1);
	dirlist = lcuT_newobjreq(L, DirectoryList);
	luaL_setmetatable(L, LCU_DIRECTORYLISTCLS);
	if (noyield) {
		uv_loop_t *loop = lcu_toloop(sched);
		int err = uv_fs_scandir(loop, &dirlist->filereq, path, 0, NULL);
		return lcuL_pushresults(L, 1, err);
	} else {
		return lcuT_resetobjreqopk(L, sched, (lcu_ObjectRequest *)dirlist,
		                              k_setuplistdir, returnlistdir, NULL);
	}
}


/*
 * Files
 */

/* succ [, errmsg] = system.openfile (path [, mode [, perm]]) */
static int returnopenfile (lua_State *L) {
	uv_file *file = (uv_file *)lua_touserdata(L, 2);
	ssize_t result = (ssize_t)lua_tointeger(L, 3);
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
	const char *path = lua_tostring(L, 1);
	int flags = (int)lua_tointeger(L, 3);
	int perm = (int)lua_tointeger(L, 4);
	int err = uv_fs_open(loop, filereq, path, flags, perm, on_fileopen);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_pop(L, 2);  /* discard 'flags' and 'perm' */
	return -1;  /* yield on success */
}
static int system_openfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *path = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "r");
	int flags = 0;
	int perm = 0;
	int noyield = 0;
	for (; *mode == LCU_NOYIELDMODE; mode++) noyield = 1;
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
		case LCU_NOYIELDMODE: return luaL_error(L, "'%c' must be in the begin of 'mode'", *mode);
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
	switch (perm) {
		case 1: flags |= UV_FS_O_RDONLY; break;
		case 2: flags |= UV_FS_O_WRONLY; break;
		case 3: flags |= UV_FS_O_RDWR; break;
	}
	if (flags&UV_FS_O_CREAT) perm = checkperm(L, 3);
	else perm = 0;
	lua_settop(L, 1);
	lua_newuserdatauv(L, sizeof(uv_file), 1);  /* raise memory errors */
	lua_pushvalue(L, lua_upvalueindex(1));  /* push scheduler */
	lua_setiuservalue(L, 2, 1);
	if (noyield) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		int err = uv_fs_open(loop, &filereq, path, flags, perm, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		uv_fs_req_cleanup(&filereq);
		return returnopenfile(L);
	} else {
		lua_pushinteger(L, flags);
		lua_pushinteger(L, perm);
		return lcuT_resetreqopk(L, sched, k_setupfile, returnopenfile, NULL);
	}
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

/* __gc(file) */
static int file_gc (lua_State *L) {
	uv_file *file = checkfile(L);
	if (*file >= 0) closefile(L, file);
	return 0;
}

/* true = file:close() */
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

/* ... = file:info(mode) */
static int returnfobjinfo (lua_State *L) {
	uv_fs_t *filereq = (uv_fs_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if (filereq->result >= 0) {
		const char *mode = (const char *)lua_touserdata(L, 3);
		for (; *mode; mode++) pushfilestat(L, *mode, filereq);
		uv_fs_req_cleanup(filereq);
		return lua_gettop(L)-3;
	} else {
		ssize_t result = filereq->result;
		uv_fs_req_cleanup(filereq);
		return lcu_error(L, result);
	}
}
static int k_setupfobjinfo (lua_State *L, uv_req_t *request, uv_loop_t *loop) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = (uv_file *)lua_touserdata(L, 1);
	int err = uv_fs_fstat(loop, filereq, *file, on_fileinfo);
	if (err < 0) return lcu_error(L, err);
	return -1;  /* yield on success */
}
static int file_info (lua_State *L) {
	uv_file file = *openedfile(L);
	lcu_Scheduler *sched = tosched(L);
	int bits = INFO_NOYIELD|INFO_STAT;
	const char *mode = checkinfomode(L, 2, &bits);
	if (bits&INFO_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		int err = uv_fs_fstat(loop, &filereq, file, NULL);
		if (err < 0) return lcu_error(L, err);
		for (; *mode; mode++) pushfilestat(L, *mode, &filereq);
		return lua_gettop(L)-2;
	}
	lua_pushlightuserdata(L, (void *)mode);
	return lcuT_resetreqopk(L, sched, k_setupfobjinfo, returnfobjinfo, NULL);
}


LCUI_FUNC void lcuM_addfilef (lua_State *L) {
	static const luaL_Reg dirinfomt[] = {
		{"__gc", dirinfo_gc},
		{"__close", dirinfo_gc},
		{"__call", dirinfo_call},
		{NULL, NULL}
	};
	static const luaL_Reg filemt[] = {
		{"__gc", file_gc},
		{"__close", file_gc},
		{NULL, NULL}
	};
	static const luaL_Reg filef[] = {
		{"close", file_close},
		{"read", file_read},
		{"write", file_write},
		{"info", file_info},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"fileinfo", system_fileinfo},
		{"listdir", system_listdir},
		{"openfile", system_openfile},
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

	lua_pushliteral(L, "filebits");
	lua_createtable(L, 0, 12);
	for (i = 0; bits[i].name; i++) {
		lua_pushstring(L, bits[i].name);
		lua_pushinteger(L, bits[i].value);
		lua_settable(L, -3);
	};
	lua_settable(L, -3);

	luaL_newmetatable(L, LCU_DIRECTORYLISTCLS);
	luaL_setfuncs(L, dirinfomt, 0);
	lua_pop(L, 1);  /* pop metatable */

	lcuM_setfuncs(L, modf, LCU_MODUPVS);
}
