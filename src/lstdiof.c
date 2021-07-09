#include "lmodaux.h"
#include "loperaux.h"
#include "lsckdefs.h"
#include "lttyaux.h"


LCUI_FUNC void lcuM_addstdiof (lua_State *L) {
	static const char *const field[] = { "stdin", "stdout", "stderr" };
	lcu_Scheduler *sched = (lcu_Scheduler *)lua_touserdata(L, -2);
	uv_loop_t *loop = lcu_toloop(sched);
	int *stdiofd = lcuTY_tostdiofd(L);
	int i;
	for (i = 0; i < LCU_STDIOFDCOUNT; i++) {
		int fd = stdiofd[i];
		uv_handle_type type = uv_guess_handle(fd);
		lcu_UdataHandle *udhdl = NULL;
		int err;
		switch (type) {
			case UV_TTY: {
				udhdl = lcuT_createudhdl(L, -2, sizeof(lcu_TermSocket), LCU_TERMSOCKETCLS);
				err = uv_tty_init(loop, (uv_tty_t *)lcu_ud2hdl(udhdl), fd, 0);
				if (err >= 0) udhdl->flags = 0;
			} break;
			case UV_TCP: {
				uv_tcp_t *tcp;
				udhdl = lcuT_createudhdl(L, -2, sizeof(lcu_TcpSocket), LCU_TCPACTIVECLS);
				tcp = (uv_tcp_t *)lcu_ud2hdl(udhdl);
				err = uv_tcp_init_ex(loop, tcp, AF_UNSPEC);
				if (err >= 0) {
					err = uv_tcp_open(tcp, fd);
					if (err >= 0) {
						struct sockaddr_storage addr;
						int addrsz = sizeof(addr);
						err = uv_tcp_getsockname(tcp, (struct sockaddr *)&addr, &addrsz);
						if (err >= 0) udhdl->flags = (addr.ss_family == AF_INET6) ?
						                             LCU_SOCKIPV6FLAG : 0;
					}
				}
				else lua_pushnil(L);
			} break;
			case UV_UDP: {
				uv_udp_t *udp;
				udhdl = lcuT_createudhdl(L, -2, sizeof(lcu_UdpSocket), LCU_UDPSOCKETCLS);
				udp = (uv_udp_t *)lcu_ud2hdl(udhdl);
				err = uv_udp_init_ex(loop, udp, AF_UNSPEC);
				if (err >= 0) {
					err = uv_udp_open(udp, fd);
					if (err >= 0) {
						struct sockaddr_storage addr;
						int addrsz = sizeof(addr);
						err = uv_udp_getsockname(udp, (struct sockaddr *)&addr, &addrsz);
						if (err >= 0) udhdl->flags = (addr.ss_family == AF_INET6) ?
						                             LCU_SOCKIPV6FLAG : 0;
					}
				}
				else lua_pushnil(L);
			} break;
			case UV_NAMED_PIPE: {
				uv_pipe_t *pipe;
				udhdl = lcuT_createudhdl(L, -2, sizeof(lcu_PipeSocket), LCU_PIPESHARECLS);
				pipe = (uv_pipe_t *)lcu_ud2hdl(udhdl);
				err = uv_pipe_init(loop, pipe, 1);
				if (err >= 0) {
					err = uv_pipe_open(pipe, fd);
					if (err >= 0) udhdl->flags = LCU_SOCKTRANFFLAG;
					else if (err == UV_EINVAL) {
						err = uv_pipe_init(loop, pipe, 0);
						if (err >= 0) {
							err = uv_pipe_open(pipe, fd);
							if (err >= 0) udhdl->flags = 0;
						}
					}
				}
			} break;
			case UV_FILE: {
				uv_file *file = (uv_file *)lua_newuserdatauv(L, sizeof(uv_file), 1);
				*file = fd;
				lua_pushvalue(L, -3);  /* push scheduler */
				lua_setiuservalue(L, -2, 1);
				luaL_setmetatable(L, LCU_FILECLS);
				err = 0;
			} break;
			default: {
				err = UV_EAI_SOCKTYPE;
				lua_pushnil(L);
			} break;
		}
		if (err < 0) {
			lua_pop(L, 1);
			lcuL_warnerr(L, field[i], err);
		} else {
			lua_setfield(L, -2, field[i]);
		}
	}
}
