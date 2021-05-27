# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= guess

# Where to install. The installation starts in the src directory,
# so take care if INSTALL_TOP is not an absolute path. See the local target.
# You may want to make INSTALL_LMOD and INSTALL_CMOD consistent with
# LUA_ROOT, LUA_LDIR, and LUA_CDIR in luaconf.h.
INSTALL_TOP= /usr/local
INSTALL_CMOD= $(INSTALL_TOP)/lib/lua/$(LUA_VER)
INSTALL_LMOD= $(INSTALL_TOP)/share/lua/$(LUA_VER)

# How to install. If your install program does not support "-p", then
# you may have to run ranlib on the installed liblua.a.
INSTALL_DATA= install -p -m 0644
#
# If you don't have "install" you can use "cp" instead.
# INSTALL_DATA= cp -p

# Other utilities.
MKDIR= mkdir -p
RM= rm -f

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

# Convenience platforms targets.
PLATS= guess generic linux macosx solaris

# What to install.
TO_CMOD= coutil.so
TO_LMOD= \
coutil/mutex.lua \
coutil/queued.lua \
coutil/event.lua \
coutil/promise.lua \
coutil/spawn.lua

# Lua version and release.
LUA_VER= 5.4

# Targets start here.
all: $(PLAT)

$(PLATS) help clean:
	@cd src && $(MAKE) $@

install:
	cd src && $(MKDIR) $(INSTALL_CMOD) $(INSTALL_LMOD)/coutil
	cd src && $(INSTALL_DATA) $(TO_CMOD) $(INSTALL_CMOD)
	cd lua && $(INSTALL_DATA) $(TO_LMOD) $(INSTALL_LMOD)/coutil

uninstall:
	cd src && cd $(INSTALL_CMOD) && $(RM) $(TO_CMOD)
	cd lua && cd $(INSTALL_LMOD) && $(RM) $(TO_LMOD)

local:
	$(MAKE) install INSTALL_TOP=../install

# make may get confused with install/ if it does not support .PHONY.
dummy:

# Echo config parameters.
echo:
	@cd src && $(MAKE) -s echo
	@echo "PLAT= $(PLAT)"
	@echo "LUA_VER= $LUA_VER"
	@echo "TO_CMOD= $(TO_CMOD)"
	@echo "TO_LMOD= $(TO_LMOD)"
	@echo "INSTALL_CMOD= $(INSTALL_CMOD)"
	@echo "INSTALL_LMOD= $(INSTALL_LMOD)"
	@echo "INSTALL_DATA= $(INSTALL_DATA)"

# Echo pkg-config data.
pc:
	@echo "prefix=$(INSTALL_TOP)"
	@echo "libdir=$(INSTALL_LIB)"
	@echo "includedir=$(INSTALL_INC)"

# Targets that do not create files (not all makes understand .PHONY).
.PHONY: all $(PLATS) help clean install install_lib install_mod \
        uninstall uninstall_lib uninstall_mod local dummy echo pc

# (end of Makefile)
