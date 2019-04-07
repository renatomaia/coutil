# See ../doc/readme.md for installation and 
# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

LUA_HOME= /home/maia/Work/lua/build/5.3
LUA_INC= $(LUA_HOME)/include
LUA_LIB= $(LUA_HOME)/lib
LUA_BIN= $(LUA_HOME)/bin
LUA_VER= 5.3

LIB_A=	libluacoutil.a
SCH_M=	scheduler.so

LD= ld
CC= gcc -std=gnu99
CFLAGS= -g -Wall -I. -I$(LUA_INC) $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= -L$(LUA_LIB) $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -luv $(SYSLIBS) $(MYLIBS)

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

SCH_O=	lscheduv.o

ALL_O=	$(SCH_O) $(MYOBJS)
ALL_A= $(LIB_A)
ALL_M= $(SCH_M)
ALL_T= $(ALL_A) $(ALL_M)

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

m:	$(ALL_M)

$(SCH_M): $(SCH_O)
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
