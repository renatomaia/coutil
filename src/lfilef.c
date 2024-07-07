#include "lmodaux.h"
#include "loperaux.h"

#include <string.h>
#ifdef __linux__
#include </usr/include/linux/magic.h>
#endif
#include <luamem.h>


#if !LCU_LIBUVMINVER(8)
#define uv_fs_realpath(L,R,P,C)	((void)(L),(void)(R),(void)(P),(void)(C),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(14)
#define uv_fs_copyfile(L,R,S,D,F,C)	((void)(L),(void)(R),(void)(S),(void)(D),(void)(F),(void)(C),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(21)
#define uv_fs_lchown(L,R,P,U,G,C)	((void)(L),(void)(R),(void)(P),(void)(U),(void)(G),(void)(C),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(31)
#define uv_fs_statfs(L,R,P,C)	((void)(L),(void)(R),(void)(P),(void)(C),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(34)
#define uv_fs_mkstemp(L,R,T,C)	((void)(L),(void)(R),(void)(T),(void)(C),UV_ENOSYS)
#endif
#if !LCU_LIBUVMINVER(36)
#define uv_fs_lutime(L,R,P,A,M,C)	((void)(L),(void)(R),(void)(P),(void)(A),(void)(M),(void)(C),UV_ENOSYS)
#endif


#ifdef _WIN32
#define S_IFIFO _S_IFIFO
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#endif

static int checkperm (lua_State *L, int arg) {
	if (lua_type(L, arg) == LUA_TSTRING) {
		const char *mode = lua_tostring(L, arg);
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
#ifdef S_IFSOCK
		case S_IFSOCK: lua_pushliteral(L, "socket"); break;
#endif
		case S_IFLNK: lua_pushliteral(L, "link"); break;
		case S_IFREG: lua_pushliteral(L, "file"); break;
#ifdef S_IFBLK
		case S_IFBLK: lua_pushliteral(L, "block"); break;
#endif
		case S_IFDIR: lua_pushliteral(L, "directory"); break;
		case S_IFCHR: lua_pushliteral(L, "character"); break;
#ifdef S_IFIFO
		case S_IFIFO: lua_pushliteral(L, "pipe"); break;
#endif
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
		case 'D': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_dev); break;
		case '#': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_ino); break;
		case '*': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_nlink); break;
		case 'u': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_uid); break;
		case 'g': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_gid); break;
		case 'd': lua_pushinteger(L, (lua_Integer)filereq->statbuf.st_rdev); break;
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

static void on_fileopdone (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	uv_fs_req_cleanup(filereq);
	if (thread) {
		int nret;
		if (filereq->result < 0) {
			nret = lcuL_pusherrres(thread, filereq->result);
		} else {
			lua_pushinteger(thread, filereq->result);
			nret = 1;
		}
		lcuU_resumecoreq(loop, request, nret);
	}
}

static int returntrueover1 (lua_State *L) {
	int top = lua_gettop(L);
	if (top > 2) return top-1;
	lua_pushboolean(L, 1);
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
	int i;
	switch (bits>>INFO_BITCOUNT) {  /* get current source */
		case INFO_READLINK: {
			for (i = 0; mode[i]; i++) if (mode[i] == '=') {
				if (filereq->result == UV_EINVAL) lua_pushnil(L);
				else lua_pushstring(L, (const char *)filereq->ptr);
				placeat(L, i+1+base);
			}
		} break;
		case INFO_REALPATH: {
			for (i = 0; mode[i]; i++) if (mode[i] == 'p') {
				lua_pushstring(L, (const char *)filereq->ptr);
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
		case 'D':
		case '#':
		case '*':
		case 'u':
		case 'g':
		case 'd':
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
		case 'l': if (!(mask&INFO_LSTAT)) break;  /* FALLTHRU */
		case LCU_NOYIELDMODE:
			return luaL_error(L, "'%c' must be in the begin of 'mode'", mode);
	}
	return 0;
}

static const char *checkprefixflags (const char *mode, int *bits) {
	int mask = *bits;
	*bits = 0;
	for (; *mode; mode++) switch (*mode) {
		case LCU_NOYIELDMODE:
			*bits |= INFO_NOYIELD;
			break;
		case 'l':
			if (!(mask&INFO_LSTAT)) return mode;
			*bits |= INFO_LSTAT;
			break;
		default:
			return mode;
	}
	return mode;
}

static const char *checkinfomode (lua_State *L, int arg, int *bits) {
	int i, mask = *bits;
	const char *mode = luaL_checkstring(L, arg);
	mode = checkprefixflags(mode, bits);
	for (i = 0; mode[i]; i++) {
		int sel = toinfosrc(L, mode[i], mask);
		if (sel&mask) *bits |= sel;
		else luaL_error(L, "unknown mode char (got '%c')", mode[i]);
	}
	lua_settop(L, arg);
	luaL_checkstack(L, i+arg+5, "too many values");  /* 2=lua_push, 3=tothrop() */
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

static int getinfosrc (int *bits,
                       uv_loop_t *loop,
                       uv_fs_t *filereq,
                       const char *path,
                       uv_fs_cb callback) {
	int err = UV_EAI_FAIL;
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
	return err;
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
		uv_fs_req_cleanup(filereq);
		return lcu_error(L, filereq->result);
	}
}
static void on_fileinfo (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endcoreq(loop, request);
	if (thread) {
		lua_pushlightuserdata(thread, filereq);
		lcuU_resumecoreq(loop, request, 1);
	}
	else uv_fs_req_cleanup(filereq);
}
static int k_setupfileinfo (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int bits = lua_tointeger(L, 3);
	int err = getinfosrc(&bits, loop, filereq, path, on_fileinfo);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) lcu_error(L, err);
	lua_pushinteger(L, bits);
	lua_replace(L, 3);
	return -1;  /* yield on success */
}
static int yieldfileinfo (lua_State *L) {
	int bits = lua_tointeger(L, 3);
	if (bits&INFO_SRCMASK) {
		lcu_Scheduler *sched = lcu_getsched(L);
		return lcuT_resetcoreqk(L, sched, k_setupfileinfo, returnfileinfo, NULL);
	}
	return lua_gettop(L)-4;
}
static int system_fileinfo (lua_State *L) {
	int bits = INFO_LSTAT|INFO_SRCMASK;  /* accept all sources */
	const char *mode = checkinfomode(L, 2, &bits);
	if (bits&INFO_NOYIELD) {
		lcu_Scheduler *sched = lcu_getsched(L);
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		while (bits&INFO_SRCMASK) {
			int err = getinfosrc(&bits, loop, &filereq, path, NULL);
			if (err < 0) lcu_error(L, err);
			pushfileinfo(L, 2, bits, mode, &filereq);
			uv_fs_req_cleanup(&filereq);
		}
		return lua_gettop(L)-2;
	}
	lua_pushinteger(L, bits);
	lua_pushlightuserdata(L, (void *)mode);
	return yieldfileinfo(L);
}


static void checktouchargs (lua_State *L,
                            int arg,
                            int *bits,
                            lua_Number *atime,
                            lua_Number *mtime) {
	int i, mask = *bits;
	const char *mode = luaL_checkstring(L, arg);
	mode = checkprefixflags(mode, bits);
	*atime = -1;
	*mtime = -1;
	for (i = 0; mode[i]; i++) {
		lua_Number time = luaL_checknumber(L, 3+i);
		luaL_argcheck(L, time >= 0, 3+i, "invalid time");
		switch (mode[i]) {
			case 'a': *atime = time; break;
			case 'm': *mtime = time; break;
			case 'b': *atime = *mtime = time; break;
			case 'l': if (!(mask&INFO_LSTAT)) goto badchar;  /* FALLTHRU */
			case LCU_NOYIELDMODE:
				luaL_error(L, "'%c' must be in the begin of 'mode'", mode[i]);
				return;
			default: badchar:
				luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 3+i, mode[i]);
				return;
		}
	}
	if (*atime == -1 || *mtime == -1) {
		lua_Number time;
		uv_timeval64_t timeval;
		int err = uv_gettimeofday(&timeval);
		if (err < 0) lcu_error(L, err);
		time = lcu_time2sec(timeval);
		if (*atime == -1) *atime = time;
		if (*mtime == -1) *mtime = time;
	}
}

/* true = system.touchfile (path [, mode, times...]) */
#define callutime(B,L,R,P,A,M,C)	( (B&INFO_LSTAT) ? uv_fs_lutime(L,R,P,A,M,C) \
                                	                 : uv_fs_utime(L,R,P,A,M,C) )
static int k_setupfiletouch (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int bits = (int)lua_tointeger(L, 2);
	lua_Number atime = lua_tonumber(L, 3);
	lua_Number mtime = lua_tonumber(L, 4);
	int err = callutime(bits, loop, filereq, path,
	                    (double)atime, (double)mtime, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_touchfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	int bits = INFO_LSTAT;
	lua_Number atime, mtime;
	checktouchargs(L, 2, &bits, &atime, &mtime);
	if (bits&INFO_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		int err = callutime(bits, loop, &filereq, path, (double)atime, (double)mtime, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushinteger(L, bits);
	lua_pushnumber(L, atime);
	lua_pushnumber(L, mtime);
	return lcuT_resetcoreqk(L, sched, k_setupfiletouch, returntrueover1, NULL);
}


/* true = system.ownfile (path, user, group [, mode]) */
#define callchown(B,L,R,P,U,G,C)	( (B&INFO_LSTAT) ? uv_fs_lchown(L,R,P,U,G,C) \
                                	                 : uv_fs_chown(L,R,P,U,G,C) )
static int k_setupfileown (lua_State *L,
                           uv_req_t *request,
                           uv_loop_t *loop,
                           lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	uv_uid_t uid = (uv_uid_t)luaL_checkinteger(L, 2);
	uv_gid_t gid = (uv_gid_t)luaL_checkinteger(L, 3);
	int bits = (int)lua_tointeger(L, 4);
	int err = callchown(bits, loop, filereq, path, uid, gid, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_ownfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *mode = luaL_optstring(L, 4, "");
	int bits = INFO_LSTAT;
	mode = checkprefixflags(mode, &bits);
	if (*mode)
		luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 4, *mode);
	if (bits&INFO_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		uv_uid_t uid = (uv_uid_t)luaL_checkinteger(L, 2);
		uv_gid_t gid = (uv_gid_t)luaL_checkinteger(L, 3);
		int err = callchown(bits, loop, &filereq, path, uid, gid, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 3);
	lua_pushinteger(L, bits);
	return lcuT_resetcoreqk(L, sched, k_setupfileown, returntrueover1, NULL);
}


/* true = system.grantfile (path, perm [, mode]) */
static int k_setupfilegrant (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int perm = (int)lua_tointeger(L, 2);
	int err = uv_fs_chmod(loop, filereq, path, perm, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_grantfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	int perm = checkperm(L, 2);
	if (lcuL_checknoyieldmode(L, 3)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		int err = uv_fs_chmod(loop, &filereq, path, perm, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushinteger(L, perm);
	return lcuT_resetcoreqk(L, sched, k_setupfilegrant, returntrueover1, NULL);
}


/* true = system.linkfile (path, link [, mode]) */
#define MKLNK_NOYIELD   0x01
#define MKLNK_SYMBOLIC  0x02
#define callmklnk(S,L,R,P,N,F,C) (S) ? uv_fs_symlink(L,R,P,N,F,C) : uv_fs_link(L,R,P,N,C)
static int k_setupmklnk (lua_State *L,
                         uv_req_t *request,
                         uv_loop_t *loop,
                         lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	const char *link = luaL_checkstring(L, 2);
	int flags = lua_tonumber(L, 3);
	int err = callmklnk(lua_isinteger(L, 3), loop, filereq, path, link, flags,
	                    on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_linkfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *mode = luaL_optstring(L, 3, "");
	int bits = 0, flags = 0;
	for (; *mode; mode++) switch (*mode) {
		case 'd': bits |= MKLNK_SYMBOLIC; flags |= UV_FS_SYMLINK_DIR; break;
		case 'j': bits |= MKLNK_SYMBOLIC; flags |= UV_FS_SYMLINK_JUNCTION; break;
		case 's': bits |= MKLNK_SYMBOLIC; break;
		case LCU_NOYIELDMODE: bits |= MKLNK_NOYIELD; break;
		default:
			return luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 3, *mode);
	}
	if (bits&MKLNK_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		const char *link = luaL_checkstring(L, 2);
		int err = callmklnk(bits&MKLNK_SYMBOLIC, loop, &filereq, path, link, flags, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 2);
	if (bits&MKLNK_SYMBOLIC) lua_pushinteger(L, flags);
	else lua_pushnil(L);
	return lcuT_resetcoreqk(L, sched, k_setupmklnk, returntrueover1, NULL);
}


/* true = system.movefile (src, dst [, mode]) */
static int k_setupmvfile (lua_State *L,
                          uv_req_t *request,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *src = luaL_checkstring(L, 1);
	const char *dst = luaL_checkstring(L, 2);
	int err = uv_fs_rename(loop, filereq, src, dst, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_movefile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	if (lcuL_checknoyieldmode(L, 3)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *src = luaL_checkstring(L, 1);
		const char *dst = luaL_checkstring(L, 2);
		int err = uv_fs_rename(loop, &filereq, src, dst, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	return lcuT_resetcoreqk(L, sched, k_setupmvfile, returntrueover1, NULL);
}


/* true = system.copyfile (src, dst [, mode]) */
static int k_setupcpfile (lua_State *L,
                          uv_req_t *request,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *src = luaL_checkstring(L, 1);
	const char *dst = luaL_checkstring(L, 2);
	int flags = lua_tointeger(L, 3);
	int err = uv_fs_copyfile(loop, filereq, src, dst, flags, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_copyfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *mode = luaL_optstring(L, 3, "");
	int noyield = 0, flags = 0;
	for (; *mode; mode++) switch (*mode) {
		case 'n': flags |= UV_FS_COPYFILE_EXCL; break;
#if LCU_LIBUVMINVER(20)
		case 'c': flags |= UV_FS_COPYFILE_FICLONE; break;
		case 'C': flags |= UV_FS_COPYFILE_FICLONE_FORCE; break;
#endif
		case LCU_NOYIELDMODE: noyield = 1; break;
		default:
			return luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 3, *mode);
	}
	if (noyield) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *src = luaL_checkstring(L, 1);
		const char *dst = luaL_checkstring(L, 2);
		int err = uv_fs_copyfile(loop, &filereq, src, dst, flags, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 2);
	lua_pushinteger(L, flags);
	return lcuT_resetcoreqk(L, sched, k_setupcpfile, returntrueover1, NULL);
}


/* true = system.removefile (path [, mode]) */
#define RMFILE_NOYIELD    0x01
#define RMFILE_DIRECTORY  0x02
#define callrmfile(D,L,R,P,C)	((D) ? uv_fs_rmdir(L,R,P,C) : uv_fs_unlink(L,R,P,C))
static int k_setuprmfile (lua_State *L,
                          uv_req_t *request,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int err = callrmfile(lua_toboolean(L, 2), loop, filereq, path,
	                     on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_removefile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *mode = luaL_optstring(L, 2, "");
	int bits = 0;
	for (; *mode; mode++) switch (*mode) {
		case 'd': bits |= RMFILE_DIRECTORY; break;
		case LCU_NOYIELDMODE: bits |= RMFILE_NOYIELD; break;
		default:
			return luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 2, *mode);
	}
	if (bits&RMFILE_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		int err = callrmfile(bits&RMFILE_DIRECTORY, loop, &filereq, path, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushboolean(L, bits&RMFILE_DIRECTORY);
	return lcuT_resetcoreqk(L, sched, k_setuprmfile, returntrueover1, NULL);
}


/*
 * Creation
 */

/* true = system.maketemp (prefix [, mode]) */
#define MKTMP_TEMPLATESZ  256
#define MKTMP_NOYIELD     0x01
#define MKTMP_CREATEFILE  0x02
#define MKTMP_OPENFILE    0x06
static int callmktmp (int mkfile,
                      uv_loop_t* loop,
                      uv_fs_t* filereq,
                      const char* prefix,
                      uv_fs_cb callback) {
	char template[MKTMP_TEMPLATESZ];
	int i;
	for (i = 0; (template[i] = *prefix); prefix++, i++);
	strcpy(template+i, "XXXXXX");
	return mkfile ? uv_fs_mkstemp(loop, filereq, template, callback)
	              : uv_fs_mkdtemp(loop, filereq, template, callback);
}
static int pcallmktmp (lua_State *L) {
	const char *mode = (const char *)lua_touserdata(L, 2);
	uv_fs_t *filereq = (uv_fs_t *)lua_touserdata(L, 4);
	if (filereq->result < 0) return lcuL_pusherrres(L, filereq->result);
	if (mode) for (; *mode; mode++) switch (*mode) {
		case 'f': {
			lua_pushstring(L, filereq->path);
		} break;
		case 'o': {
			if (filereq->result >= 0) {
				uv_file *file = (uv_file *)lua_newuserdatauv(L, sizeof(uv_file), 1);
				lua_pushvalue(L, 3);
				lua_setiuservalue(L, -2, 1);
				*file = filereq->result;
				luaL_setmetatable(L, LCU_FILECLS);
				filereq->result = -(lua_gettop(L));
			} else {
				lua_pushvalue(L, -(filereq->result));
			}
		} break;
	}
	else lua_pushstring(L, filereq->path);
	return lua_gettop(L)-4;
}
static int returnmktmp (lua_State *L) {
	int err, mkfile = lua_isuserdata(L, 3);
	uv_fs_t *filereq = (uv_fs_t *)lua_touserdata(L, 5);
	lcu_assert(lua_gettop(L) == 5);
	if (filereq->result < 0) {
		uv_fs_req_cleanup(filereq);
		return lcuL_pusherrres(L, filereq->result);
	}
	lua_pushcfunction(L, pcallmktmp);
	lua_replace(L, 1);
	err = lua_pcall(L, 4, LUA_MULTRET, 0);
	uv_fs_req_cleanup(filereq);
	if (mkfile && filereq->result >= 0) {
		uv_fs_t closereq;
		int err = uv_fs_close(filereq->loop, &closereq, (uv_file)filereq->result, NULL);
		if (err < 0) lcuL_warnerr(L, "system.maketemp", err);
		uv_fs_req_cleanup(&closereq);
	}
	if (err != LUA_OK) return lua_error(L);
	return lua_gettop(L);
}
static int k_setupmktmp (lua_State *L,
                         uv_req_t *request,
                         uv_loop_t *loop,
                         lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *prefix = lua_tostring(L, 1);
	int err = callmktmp(lua_isuserdata(L, 3), loop, filereq, prefix,
	                    on_fileinfo);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int system_maketemp (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	size_t len;
	const char *prefix = luaL_checklstring(L, 1, &len);
	const char *mode = luaL_optstring(L, 2, "");
	int i, bits = 0;
	luaL_argcheck(L, len < MKTMP_TEMPLATESZ-6, 1, "too long");
	for (; *mode == LCU_NOYIELDMODE; mode++) bits = MKTMP_NOYIELD;
	for (i = 0; mode[i]; i++) switch (mode[i]) {
		case 'f': bits |= MKTMP_CREATEFILE; break;
		case 'o': bits |= MKTMP_OPENFILE; break;
		case LCU_NOYIELDMODE:
			return luaL_error(L, "'%c' must be in the begin of 'mode'", mode[i]);
		default:
			return luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 2, *mode);
	}
	lua_settop(L, 2);
	if (bits&MKTMP_CREATEFILE) lua_pushlightuserdata(L, (void *)mode);
	else lua_pushnil(L);
	if (bits&MKTMP_OPENFILE) lua_pushvalue(L, lua_upvalueindex(1));
	else lua_pushnil(L);
	luaL_checkstack(L, i+5, "too many values to return");
	if (bits&MKTMP_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		int err = callmktmp(bits&MKTMP_CREATEFILE, loop, &filereq, prefix, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushlightuserdata(L, &filereq);
		return returnmktmp(L);
	}
	return lcuT_resetcoreqk(L, sched, k_setupmktmp, returnmktmp, NULL);
}


/*
 * Directories
 */

/* true = system.makedir (path, perm [, mode]) */
static int k_setupmkdir (lua_State *L,
                         uv_req_t *request,
                         uv_loop_t *loop,
                         lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int perm = checkperm(L, 2);
	int err = uv_fs_mkdir(loop, filereq, path, perm, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int system_makedir (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	if (lcuL_checknoyieldmode(L, 3)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		const char *path = luaL_checkstring(L, 1);
		int perm = checkperm(L, 2);
		int err = uv_fs_mkdir(loop, &filereq, path, perm, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	return lcuT_resetcoreqk(L, sched, k_setupmkdir, returntrueover1, NULL);
}

typedef struct DirectoryList {
	lua_CFunction results;
	lua_CFunction cancel;
	uv_fs_t filereq;
} DirectoryList;

#define checkdirinfo(L)	((DirectoryList *)luaL_checkudata(L, 1, LCU_DIRECTORYLISTCLS))

static DirectoryList *openeddirinfo (lua_State *L) {
	DirectoryList *dirlist = checkdirinfo(L);
	luaL_argcheck(L, dirlist->filereq.result >= 0, 1, "closed");
	return dirlist;
}

/* getmetatable(dirinfo).__{gc,close}(dirinfo) */
static int dirinfo_gc (lua_State *L) {
	DirectoryList *dirlist = checkdirinfo(L);
	if (dirlist->filereq.result >= 0) {
		uv_fs_req_cleanup(&dirlist->filereq);
		dirlist->filereq.result = -1;
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
		dirlist->filereq.result = -1;
		return 0;
	}
	lua_pushstring(L, entry.name);
	switch (entry.type) {
		case UV_DIRENT_SOCKET: lua_pushliteral(L, "socket"); break;
		case UV_DIRENT_LINK: lua_pushliteral(L, "link"); break;
		case UV_DIRENT_FILE: lua_pushliteral(L, "file"); break;
		case UV_DIRENT_BLOCK: lua_pushliteral(L, "block"); break;
		case UV_DIRENT_DIR: lua_pushliteral(L, "directory"); break;
		case UV_DIRENT_CHAR: lua_pushliteral(L, "character"); break;
		case UV_DIRENT_FIFO: lua_pushliteral(L, "pipe"); break;
		default: lua_pushliteral(L, "unknown"); break;
	}
	return 2;
}

/* iterator, state, varinit, closing = system.listdir (path [, mode]) */
static int returnlistdir (lua_State *L) {
	DirectoryList *dirlist = (DirectoryList *)lua_touserdata(L, 1);
	int err = dirlist->filereq.result;
	if (err < 0) return lcu_error(L, err);
	lua_settop(L, 3);  /* return 2 'nil' as iterator state and control var. */
	lua_pushvalue(L, 1); /* return 'listdir' as the closing value. */
	return 4;
}
static void uv_onlistdir (uv_fs_t *filereq) {
	uv_loop_t *loop = filereq->loop;
	uv_req_t *request = (uv_req_t *)filereq;
	lua_State *thread = lcuU_endudreq(loop, request);
	if (thread) lcuU_resumeudreq(loop, request, 0);
	else {
		uv_fs_req_cleanup(filereq);
		filereq->result = -1;
	}
}
static int k_setuplistdir (lua_State *L,
                           uv_req_t *request,
                           uv_loop_t *loop,
                           lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = lua_tostring(L, 2);
	int err = uv_fs_scandir(loop, filereq, path, 0, uv_onlistdir);
	lcu_assert(op == NULL);
	if (err < 0) return lcu_error(L, err);
	lua_pop(L, 1);  /* discard 'path' */
	return -1;  /* yield on success */
}
static int system_listdir (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *path = luaL_checkstring(L, 1);
	int noyield = lcuL_checknoyieldmode(L, 2);
	DirectoryList *dirlist;
	lua_settop(L, 1);
	dirlist = lcuT_newudreq(L, DirectoryList);
	luaL_setmetatable(L, LCU_DIRECTORYLISTCLS);
	lua_insert(L, 1);
	if (noyield) {
		uv_loop_t *loop = lcu_toloop(sched);
		int err = uv_fs_scandir(loop, &dirlist->filereq, path, 0, NULL);
		if (err < 0) return lcu_error(L, err);
		lua_pop(L, 1);
		return returnlistdir(L);
	} else {
		return lcuT_resetudreqk(L, sched, (lcu_UdataRequest *)dirlist,
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
	lua_State *thread = lcuU_endcoreq(loop, request);
	uv_fs_req_cleanup(filereq);
	if (thread) {
		lua_pushinteger(thread, filereq->result);
		lcuU_resumecoreq(loop, request, 1);
	}
}
static int k_setupfile (lua_State *L,
                        uv_req_t *request,
                        uv_loop_t *loop,
                        lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	const char *path = luaL_checkstring(L, 1);
	int flags = (int)lua_tointeger(L, 3);
	int perm = (int)lua_tointeger(L, 4);
	int err = uv_fs_open(loop, filereq, path, flags, perm, on_fileopen);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_pop(L, 2);  /* discard 'flags' and 'perm' */
	return -1;  /* yield on success */
}
static int system_openfile (lua_State *L) {
	lcu_Scheduler *sched = lcu_getsched(L);
	const char *mode = luaL_optstring(L, 2, "r");
	int flags = 0, perm = 0, noyield = 0;
	for (; *mode; mode++) switch (*mode) {
		case 'r': perm |= 1; break;
		case 'w': perm |= 2; break;
		case 'a': perm |= 2; flags |= UV_FS_O_APPEND; break;
		case 'f': perm |= 2; flags |= UV_FS_O_SYNC; break;
		case 't': perm |= 2; flags |= UV_FS_O_TRUNC; break;
		case 'n': flags |= UV_FS_O_CREAT; break;
		case 'N': flags |= UV_FS_O_CREAT|UV_FS_O_EXCL; break;
#ifdef O_CLOEXEC
		case 'x': flags |= O_CLOEXEC; break;
#endif
		case LCU_NOYIELDMODE: noyield = 1; break;
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
		const char *path = luaL_checkstring(L, 1);
		int err = uv_fs_open(loop, &filereq, path, flags, perm, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		uv_fs_req_cleanup(&filereq);
		return returnopenfile(L);
	}
	lua_pushinteger(L, flags);
	lua_pushinteger(L, perm);
	return lcuT_resetcoreqk(L, sched, k_setupfile, returnopenfile, NULL);
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

/* __gc(file) */
static int file_gc (lua_State *L) {
	uv_file *file = checkfile(L);
	if (*file >= 0) {
		uv_fs_t filereq;
		int err = uv_fs_close(lcu_toloop(tosched(L)), &filereq, *file, NULL);
		if (err >= 0) *file = -1;
		else lcuL_warnerr(L, "__gc(file)", err);
	}
	return 0;
}

/* true = file:close([mode]) */
static int k_setupclosef (lua_State *L,
                          uv_req_t *request,
                          uv_loop_t *loop,
                          lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file *file = openedfile(L);
	int err = uv_fs_close(loop, filereq, *file, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	*file = -1;
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_close (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 2)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file *file = openedfile(L);
		int err = uv_fs_close(loop, &filereq, *file, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		*file = -1;
		lua_pushboolean(L, 1);
		return 1;
	}
	return lcuT_resetcoreqk(L, sched, k_setupclosef, returntrueover1, NULL);
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
static int k_setupreadfile (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int err = callread(L, loop, filereq, file, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_read (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 6)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = callread(L, loop, &filereq, file, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		return 1;
	}
	return lcuT_resetcoreqk(L, sched, k_setupreadfile, NULL, NULL);
}

/* bytes [, err] = file:write(data [, i [, j [, offset [, mode]]]]) */
static int callwrite (lua_State *L,
                      uv_loop_t* loop,
                      uv_fs_t* filereq,
                      uv_file file,
                      uv_fs_cb callback) {
	uv_file *srcf = (uv_file *)luaL_testudata(L, 2, LCU_FILECLS);
	if (srcf) {
		int64_t offset = (int64_t)luaL_checkinteger(L, 3)-1;
		int64_t length = (int64_t)luaL_checkinteger(L, 4)-offset;
		luaL_argcheck(L, length > 0, 4, "invalid range");
		return uv_fs_sendfile(loop, filereq, file, *srcf, offset, length, callback);
	} else {
		uv_buf_t buf;  /* args from 2 to 4 (data, i, j) */
		int64_t offset = (int64_t)luaL_optinteger(L, 5, -1);
		lcu_getinputbuf(L, 2, &buf);
		return uv_fs_write(loop, filereq, file, &buf, 1, offset, callback);
	}
}
static int k_setupwritefile (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int err = callwrite(L, loop, filereq, file, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	return -1;  /* yield on success */
}
static int file_write (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 6)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = callwrite(L, loop, &filereq, file, NULL);
		if (err < 0) return lcuL_pusherrres(L, err);
		lua_pushinteger(L, filereq.result);
		return 1;
	}
	return lcuT_resetcoreqk(L, sched, k_setupwritefile, NULL, NULL);
}

/* true = file:resize (length [, mode]) */
static int k_setupfresize (lua_State *L,
                           uv_req_t *request,
                           uv_loop_t *loop,
                           lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int64_t length = (int64_t)luaL_checkinteger(L, 2);
	int err = uv_fs_ftruncate(loop, filereq, file, length, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_resize (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 3)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int64_t length = (int64_t)luaL_checkinteger(L, 2);
		int err = uv_fs_ftruncate(loop, &filereq, file, length, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	return lcuT_resetcoreqk(L, sched, k_setupfresize, returntrueover1, NULL);
}

/* true = file:flush ([mode]) */
#define FLUSH_NOYIELD   0x01
#define FLUSH_DATAONLY  0x02
#define callfsync(D,L,R,F,C) (D) ? uv_fs_fdatasync(L,R,F,C) : uv_fs_fsync(L,R,F,C)
static int k_setupfobjflush (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int err = callfsync(lua_toboolean(L, 2), loop, filereq, file, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_flush (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	const char *mode = luaL_optstring(L, 2, "");
	int bits = 0;
	for (; *mode; mode++) switch (*mode) {
		case 'd': bits |= FLUSH_DATAONLY; break;
		case LCU_NOYIELDMODE: bits |= FLUSH_NOYIELD; break;
		default:
			return luaL_error(L, "bad argument #%d, unknown mode char (got '%c')", 3, *mode);
	}
	if (bits&FLUSH_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = callfsync(bits&FLUSH_DATAONLY, loop, &filereq, file, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushboolean(L, bits&FLUSH_DATAONLY);
	return lcuT_resetcoreqk(L, sched, k_setupfobjflush, returntrueover1, NULL);
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
	}
	uv_fs_req_cleanup(filereq);
	return lcu_error(L, filereq->result);
}
static int k_setupfobjinfo (lua_State *L,
                            uv_req_t *request,
                            uv_loop_t *loop,
                            lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int err = uv_fs_fstat(loop, filereq, file, on_fileinfo);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcu_error(L, err);
	return -1;  /* yield on success */
}
static int file_info (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	int bits = INFO_STAT;  /* no extra option */
	const char *mode = checkinfomode(L, 2, &bits);
	if (bits&INFO_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = uv_fs_fstat(loop, &filereq, file, NULL);
		if (err < 0) return lcu_error(L, err);
		for (; *mode; mode++) pushfilestat(L, *mode, &filereq);
		return lua_gettop(L)-2;
	}
	lua_pushlightuserdata(L, (void *)mode);
	return lcuT_resetcoreqk(L, sched, k_setupfobjinfo, returnfobjinfo, NULL);
}

/* true = file:touch ([mode, times...]) */
static int k_setupfobjtouch (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	lua_Number atime = lua_tonumber(L, 2);
	lua_Number mtime = lua_tonumber(L, 3);
	int err = uv_fs_futime(loop, filereq, file, (double)atime, (double)mtime, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_touch (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	int bits = 0;
	lua_Number atime, mtime;
	checktouchargs(L, 2, &bits, &atime, &mtime);
	if (bits&INFO_NOYIELD) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = uv_fs_futime(loop, &filereq, file, (double)atime, (double)mtime, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushnumber(L, atime);
	lua_pushnumber(L, mtime);
	return lcuT_resetcoreqk(L, sched, k_setupfobjtouch, returntrueover1, NULL);
}

/* true = file:own (user, group [, mode]) */
static int k_setupfobjown (lua_State *L,
                           uv_req_t *request,
                           uv_loop_t *loop,
                           lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	uv_uid_t uid = (uv_uid_t)luaL_checkinteger(L, 2);
	uv_gid_t gid = (uv_gid_t)luaL_checkinteger(L, 3);
	int err = uv_fs_fchown(loop, filereq, file, uid, gid, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_own (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	if (lcuL_checknoyieldmode(L, 4)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		uv_uid_t uid = (uv_uid_t)luaL_checkinteger(L, 2);
		uv_gid_t gid = (uv_gid_t)luaL_checkinteger(L, 3);
		int err = uv_fs_fchown(loop, &filereq, file, uid, gid, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	return lcuT_resetcoreqk(L, sched, k_setupfobjown, returntrueover1, NULL);
}

/* true = file:grant (perm [, mode]) */
static int k_setupfobjgrant (lua_State *L,
                             uv_req_t *request,
                             uv_loop_t *loop,
                             lcu_Operation *op) {
	uv_fs_t *filereq = (uv_fs_t *)request;
	uv_file file = *openedfile(L);
	int perm = (int)lua_tointeger(L, 2);
	int err = uv_fs_fchmod(loop, filereq, file, perm, on_fileopdone);
	lcuT_armcoreq(L, loop, op, err);
	if (err < 0) return lcuL_pusherrres(L, err);
	lua_settop(L, 1);
	return -1;  /* yield on success */
}
static int file_grant (lua_State *L) {
	lcu_Scheduler *sched = tosched(L);
	int perm = checkperm(L, 2);
	if (lcuL_checknoyieldmode(L, 3)) {
		uv_loop_t *loop = lcu_toloop(sched);
		uv_fs_t filereq;
		uv_file file = *openedfile(L);
		int err = uv_fs_fchmod(loop, &filereq, file, perm, NULL);
		return lcuL_pushresults(L, 0, err);
	}
	lua_settop(L, 1);
	lua_pushinteger(L, perm);
	return lcuT_resetcoreqk(L, sched, k_setupfobjgrant, returntrueover1, NULL);
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
		{"resize", file_resize},
		{"flush", file_flush},
		{"info", file_info},
		{"touch", file_touch},
		{"own", file_own},
		{"grant", file_grant},
		{NULL, NULL}
	};
	static const luaL_Reg modf[] = {
		{"fileinfo", system_fileinfo},
		{"listdir", system_listdir},
		{"makedir", system_makedir},
		{"maketemp", system_maketemp},
		{"linkfile", system_linkfile},
		{"movefile", system_movefile},
		{"copyfile", system_copyfile},
		{"openfile", system_openfile},
		{"touchfile", system_touchfile},
		{"ownfile", system_ownfile},
		{"grantfile", system_grantfile},
		{"removefile", system_removefile},
		{NULL, NULL}
	};
	static const struct { const char *name; int value; } bits[] = {
		{ "type", S_IFMT },
#ifdef S_IFSOCK
		{ "socket", S_IFSOCK },
#endif
		{ "link", S_IFLNK },
		{ "file", S_IFREG },
#ifdef S_IFBLK
		{ "block", S_IFBLK },
#endif
		{ "directory", S_IFDIR },
		{ "character", S_IFCHR },
#ifdef S_IFIFO
		{ "pipe", S_IFIFO },
#endif
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
