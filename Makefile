include config.mk

ifneq ($(MAKE_WRAPPER),y)
$(error Run ./configure to generate wrapper and ./makew to use Makefile)
endif


export QUIET	:= @
export VM		:= qemu
export PLATFORM := $(subst $\",,$(CONFIG_COMPILER_HOST))
export ROOTDIR	:= $(shell pwd)
export SYSROOT  := $(ROOTDIR)/$(subst $\",,$(CONFIG_SYSTEM_PATH_SYSROOT))


PROJECTS := src/core src/drivers
TARGET	 := aplus.img



BUILDALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i; done
INSTALLALL: $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install; done
CLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean; done
DISTCLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i distclean; done


all: $(TARGET)
$(TARGET): BUILDALL

install: $(TARGET) INSTALLALL
	$(QUIET)./extra/utils/gen-grubcfg $(SYSROOT)
	$(QUIET)./extra/utils/gen-image $(SYSROOT) $(TARGET)

run: install
	$(QUIET)./extra/utils/run-$(VM) $(PLATFORM)

clean: CLEANALL
distclean: clean DISTCLEANALL
	$(QUIET)$(RM) $(TARGET) config.mk config.mk.old config.h makew 
	$(QUIET)$(RM) -r docs/html docs/man


.PHONY: docs distdocs
docs:
	@doxygen docs/Doxyfile
distdocs: docs
	@tar cJf aplus-docs.tar.xz docs/man docs/html