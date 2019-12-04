include config.mk

export QUIET	:= @
export PLATFORM := $(subst $\",,$(CONFIG_COMPILER_HOST))
export ROOTDIR	:= $(shell pwd)
export SYSROOT  := $(ROOTDIR)/$(subst $\",,$(CONFIG_SYSTEM_PATH_SYSROOT))


PROJECTS := src/core
TARGET	 := aplus.img



BUILDALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i; done
INSTALLALL: $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install; done
CLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean; done
DISTCLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean; done


all: $(TARGET)
$(TARGET): BUILDALL

install: $(TARGET) INSTALLALL
	$(QUIET)./extra/utils/gen-grubcfg $(SYSROOT)
	$(QUIET)./extra/utils/gen-image $(SYSROOT) $(TARGET)

run: install
	$(QUIET)./extra/utils/run-qemu $(PLATFORM)

clean: CLEANALL
distclean: clean DISTCLEANALL
	$(QUIET)$(RM) $(TARGET) config.mk config.h makew 

