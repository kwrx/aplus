include config.mk

ifneq ($(MAKE_WRAPPER),y)
$(error Run ./configure to generate wrapper and ./makew to use Makefile)
endif


export QUIET    := @ 
export VM       := qemu
export PLATFORM := $(subst $\",,$(CONFIG_COMPILER_HOST))
export ROOTDIR  := $(shell pwd)
export SYSROOT  := $(ROOTDIR)/$(subst $\",,$(CONFIG_SYSTEM_PATH_SYSROOT))


PROJECTS := kernel drivers apps
TARGET	 := aplus.img



BUILDALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i           || break; done
INSTALLALL: $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install   || break; done
CLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean     || break; done
DISTCLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i distclean || break; done


all: $(TARGET)
$(TARGET): BUILDALL

install: $(TARGET) INSTALLALL
	$(QUIET)./extra/utils/gen-grubcfg $(SYSROOT)
	$(QUIET)./extra/utils/gen-image $(SYSROOT) $(TARGET)

dist: $(TARGET) INSTALLALL
	$(QUIET)tar cJf aplus-$(PLATFORM).tar.xz $(SYSROOT) $(ROOTDIR)/docs/README.md $(ROOTDIR)/extra/utils/gen-image $(ROOTDIR)/extra/utils/gen-grubcfg $(ROOTDIR)/extra/utils/run-qemu
	$(QUIET)tar czf aplus-$(PLATFORM).tar.gz $(SYSROOT) $(ROOTDIR)/docs/README.md $(ROOTDIR)/extra/utils/gen-image $(ROOTDIR)/extra/utils/gen-grubcfg $(ROOTDIR)/extra/utils/run-qemu
	$(QUIET)zip     aplus-$(PLATFORM).zip    $(SYSROOT) $(ROOTDIR)/docs/README.md $(ROOTDIR)/extra/utils/gen-image $(ROOTDIR)/extra/utils/gen-grubcfg $(ROOTDIR)/extra/utils/run-qemu


run: install
	$(QUIET)./extra/utils/run-$(VM) $(PLATFORM) $(VM_DEBUG)

run-fast:
	$(QUIET)./extra/utils/run-$(VM) $(PLATFORM) $(VM_DEBUG)

clean: CLEANALL
distclean: clean DISTCLEANALL
	$(QUIET)$(RM) $(TARGET) config.mk config.mk.old config.h makew aplus-*.tar.gz aplus-*.tar.xz aplus-*.zip
	$(QUIET)$(RM) -r docs/html docs/man sdk/toolchain
	$(QUIET)$(ROOTDIR)/extra/utils/get-pkg.py --clean 


.PHONY: docs distdocs
docs:
	@doxygen docs/Doxyfile
distdocs: docs
	@tar cJf aplus-docs.tar.xz docs/man docs/html
