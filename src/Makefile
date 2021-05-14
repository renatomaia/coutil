# See ../doc/readme.md for installation and 
# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

LUA_DIR= /usr/local
LUA_INCDIR= $(LUA_DIR)/include
LUA_LIBDIR= $(LUA_DIR)/lib
LUA_BINDIR= $(LUA_DIR)/bin
LUALIB= lua
LUA= lua

LUAMEM_DIR= $(LUA_DIR)
LUAMEM_INCDIR= $(LUAMEM_DIR)/include
LUAMEM_LIBDIR= $(LUAMEM_DIR)/lib
LUAMEM_LIB= luamemlib

LIBUV_DIR= $(LUA_DIR)
LIBUV_INCDIR= $(LIBUV_DIR)/include
LIBUV_LIBDIR= $(LIBUV_DIR)/lib
LIBUV_LIB= uv

LIB_A=	libluacoutil.a
MOD_M=	coutil.so

LD= ld
CC= gcc -std=gnu89
CFLAGS= -g -Wall -I. -I$(LUA_INCDIR) -I$(LUAMEM_INCDIR) -I$(LIBUV_INCDIR) $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= -L$(LUA_LIBDIR) -L$(LUAMEM_LIBDIR) -L$(LIBUV_LIBDIR) $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lluamemlib -luv $(SYSLIBS) $(MYLIBS)

AR= ar rcu
RANLIB= ranlib
RM= rm -f

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

MOD_O= lmodaux.o loperaux.o lchaux.o lthpool.o \
       lcommunf.o lfilef.o linfof.o lprocesf.o lscheduf.o ltimef.o \
       lchannem.o lcoroutm.o lsystemm.o lthreadm.o

ALL_O= $(MOD_O) $(MYOBJS)
ALL_A= $(LIB_A)
ALL_M= $(MOD_M)
ALL_T= $(ALL_A) $(ALL_M)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

m:	$(ALL_M)

$(MOD_M): $(MOD_O)
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

$(LIB_A): $(ALL_O)
	$(AR) $@ $?
	$(RANLIB) $@

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"

# Convenience targets for popular platforms
ALL= all

none:
	@echo "Please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"

generic: $(ALL)

linux:
	$(MAKE) $(ALL) LD="gcc" SYSCFLAGS="-fPIC" SYSLDFLAGS="-shared"

macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-fno-common" \
	               SYSLDFLAGS="-bundle -undefined dynamic_lookup" \
	               CC='export MACOSX_DEPLOYMENT_TARGET="10.3"; gcc' \
	               LD='export MACOSX_DEPLOYMENT_TARGET="10.3"; gcc'

solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-fpic" SYSLDFLAGS="-O -shared"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a so clean depend echo none
