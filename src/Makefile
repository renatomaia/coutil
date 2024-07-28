# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= guess

LUA_DIR= /usr/local
LUA_INCDIR= $(LUA_DIR)/include

LUAMEM_DIR= $(LUA_DIR)
LUAMEM_INCDIR= $(LUAMEM_DIR)/include
LUAMEM_LIBDIR= $(LUAMEM_DIR)/lib
LUAMEM_LIB= luamem

LIBUV_DIR= $(LUA_DIR)
LIBUV_INCDIR= $(LIBUV_DIR)/include
LIBUV_LIBDIR= $(LIBUV_DIR)/lib
LIBUV_LIB= uv

LD= ld
CC= gcc -std=gnu89
CFLAGS= -O2 -Wall -Wextra -DLCU_ENABLESTDIODUP -I. \
        -I$(LUA_INCDIR) -I$(LUAMEM_INCDIR) -I$(LIBUV_INCDIR) \
        $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= -L$(LUAMEM_LIBDIR) -L$(LIBUV_LIBDIR) $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -l$(LUAMEM_LIB) -l$(LIBUV_LIB) $(SYSLIBS) $(MYLIBS)

AR= ar rcu
RANLIB= ranlib
RM= rm -f
UNAME= uname

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= guess generic linux macosx solaris

LIB_A= libluacoutil.a
MOD_M= coutil.so
MOD_O= lmodaux.o loperaux.o lchaux.o lthpool.o lttyaux.o \
       lcommunf.o lfilef.o linfof.o lprocesf.o lscheduf.o lstdiof.o ltimef.o \
       lchannem.o lcoroutm.o lsystemm.o lthreadm.o
TST_O= ../test/ltestm.o
TST_M= ../test/coutil_test.so

ALL_O= $(MOD_O)
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

$(TST_M): $(TST_O) $(LIB_S)
	$(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

test: $(TST_M)
	cd ../test && env LD_LIBRARY_PATH=$(LUAMEM_LIBDIR) LUA_CPATH="../src/?.so;./?.so;;" $(LUA_DIR)/bin/lua ./testall.lua

clean:
	$(RM) $(ALL_T) $(ALL_O) $(TST_O) $(TST_M)

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
	@echo "UNAME= $(UNAME)"

fromrockspec:
	@cd src && $(MAKE) $(PLAT)

# Convenience targets for popular platforms
ALL= all

help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"

guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

generic: $(ALL)

Linux linux:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX -D_GNU_SOURCE -fPIC" \
	               SYSLDFLAGS="-shared"

Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_MACOSX -fno-common" \
	               SYSLDFLAGS="-bundle -undefined dynamic_lookup" \
	               CC='export MACOSX_DEPLOYMENT_TARGET="10.10"; gcc' \
	               LD='export MACOSX_DEPLOYMENT_TARGET="10.10"; gcc'

SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-fpic" SYSLDFLAGS="-O -shared"

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a so clean depend echo fromrockspec
