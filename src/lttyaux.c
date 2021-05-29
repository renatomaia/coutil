#include "lttyaux.h"
#include "lmodaux.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <uv.h>

#if defined(_AIX) || \
defined(__APPLE__) || \
defined(__DragonFly__) || \
defined(__FreeBSD__) || \
defined(__FreeBSD_kernel__) || \
defined(__linux__) || \
defined(__OpenBSD__) || \
defined(__NetBSD__)
#define uv__cloexec uv__cloexec_ioctl
#else
#define uv__cloexec uv__cloexec_fcntl
#endif

#if !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(__HAIKU__)
#include <sys/ioctl.h>
int uv__cloexec_ioctl(int fd, int set) {
	int r;

	do
		r = ioctl(fd, set ? FIOCLEX : FIONCLEX);
	while (r == -1 && errno == EINTR);

	return r == -1;
}
#endif

int uv__cloexec_fcntl(int fd, int set) {
	int flags;
	int r;

	do
		r = fcntl(fd, F_GETFD);
	while (r == -1 && errno == EINTR);

	if (r == -1)
		return 1;

	/* Bail out now if already set/clear. */
	if (!!(r & FD_CLOEXEC) == !!set)
		return 0;

	if (set)
		flags = r | FD_CLOEXEC;
	else
		flags = r & ~FD_CLOEXEC;

	do
		r = fcntl(fd, F_SETFD, flags);
	while (r == -1 && errno == EINTR);

	return r == -1;
}

#ifdef O_CLOEXEC
#define uv__open_cloexec(P,F)  open(P,(F)|O_CLOEXEC)
#else  /* O_CLOEXEC */
int uv__open_cloexec(const char* path, int flags) {
	int fd = open(path, flags);
	if (fd >= 0) {
		int err = uv__cloexec(fd, 1);
		if (err) {
			uv__close(fd);
			fd = -1;
		}
	}
	return fd;
}
#endif  /* O_CLOEXEC */

int lcu_dupfd(int fd) {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__linux__)
	int newfd = uv__open_cloexec("/dev/null", 0);
	if (newfd >= 0) {
		int result = dup3(fd, newfd, O_CLOEXEC);
		if (result == -1) {
			close(newfd);
			newfd = -1;
		}
	}
#else
	int newfd = dup(fd);
	if (newfd >= 0) {
		int err = uv__cloexec(newfd, 1);
		if (err) {
			close(newfd);
			newfd = -1;
		}
	}
#endif
	return newfd;
}



static int stdiofd_gc (lua_State *L) {
	int i, *stdiofd = (int *)lua_touserdata(L, 1);
	for (i = 0; i < LCU_STDIOFDCOUNT; i++)
		if (stdiofd[i] > STDERR_FILENO)
			close(stdiofd[i]);
	return 0;
}

LCUI_FUNC int *lcuTY_tostdiofd (lua_State *L) {
	int *stdiofd;
	int type = lua_getfield(L, LUA_REGISTRYINDEX, LCU_STDIOFDREGKEY);
	if (type == LUA_TNIL) {
		int fd;
		stdiofd = (int *)lua_newuserdatauv(L, sizeof(int)*LCU_STDIOFDCOUNT, 0);
		for (fd = 0; fd < LCU_STDIOFDCOUNT; fd++) {
			stdiofd[fd] = (uv_guess_handle(fd) == UV_TTY) ? lcu_dupfd(fd) : fd;
		}
		lcuL_setfinalizer(L, stdiofd_gc);
		lua_setfield(L, LUA_REGISTRYINDEX, LCU_STDIOFDREGKEY);
	} else {
		stdiofd = (int *)lua_touserdata(L, -1);
		lcu_assert(stdiofd);
	}
	lua_pop(L, 1);
	return stdiofd;
}
