include config.mk

ifneq ($(MAKE_WRAPPER),y)
$(error Run ./configure to generate wrapper and ./makew to use Makefile)
endif



export QUIET    := @ 
export VM       := qemu
export PLATFORM := $(subst $\",,$(CONFIG_COMPILER_HOST))
export ROOTDIR  := $(shell pwd)
export SYSROOT  := $(ROOTDIR)/$(subst $\",,$(CONFIG_SYSTEM_PATH_SYSROOT))


PROJECTS := kernel-rust kernel drivers apps
TARGET	 := aplus.img

REPORT   := docs/REPORT.md


BUILDALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i           || exit 1; done
INSTALLALL: $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install   || exit 1; done
CLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean     || exit 1; done
DISTCLEANALL:   $(PROJECTS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i distclean || exit 1; done


all: $(TARGET)
$(TARGET): BUILDALL


.NOTPARALLEL:

install: $(TARGET) INSTALLALL
	$(QUIET)echo "    GEN     $(SYSROOT)/usr/lib/modules/exports"
	$(QUIET)./scripts/gen-exports $(SYSROOT) $(PLATFORM) > $(SYSROOT)/usr/lib/modules/exports
	$(QUIET)echo "    GEN     $(SYSROOT)/boot/grub.cfg"
	$(QUIET)./scripts/gen-grubcfg $(SYSROOT)
	$(QUIET)echo "    GEN     $(TARGET)"
	$(QUIET)./scripts/gen-image $(SYSROOT) $(TARGET)

install-live: $(TARGET) INSTALLALL
	$(QUIET)echo "    GEN     $(SYSROOT)/usr/lib/modules/exports"
	$(QUIET)./scripts/gen-exports $(SYSROOT) $(PLATFORM) > $(SYSROOT)/usr/lib/modules/exports
	$(QUIET)echo "    GEN     $(SYSROOT)/boot/grub.cfg"
	$(QUIET)./scripts/gen-grubcfg --live $(SYSROOT)
	$(QUIET)echo "    GEN     initrd.img"
	$(QUIET)./scripts/gen-initrd $(SYSROOT)
	$(QUIET)echo "    GEN     $(TARGET)"
	$(QUIET)./scripts/gen-image --live $(SYSROOT) $(TARGET)


dist: $(TARGET) INSTALLALL
	$(QUIET)mkdir -p                         aplus-$(PLATFORM)
	$(QUIET)mkdir -p                         aplus-$(PLATFORM)/utils
	$(QUIET)cp -r $(SYSROOT)                 aplus-$(PLATFORM)
	$(QUIET)cp -r docs/README.md             aplus-$(PLATFORM)
	$(QUIET)cp -r requirements.txt      	 aplus-$(PLATFORM)
	$(QUIET)cp -r scripts/run-qemu       	 aplus-$(PLATFORM)/utils
	$(QUIET)cp -r scripts/gen-initrd     	 aplus-$(PLATFORM)/utils
	$(QUIET)cp -r scripts/gen-grubcfg    	 aplus-$(PLATFORM)/utils
	$(QUIET)cp -r scripts/gen-image          aplus-$(PLATFORM)/utils
	$(QUIET)cp -r scripts/gen-report     	 aplus-$(PLATFORM)/utils
	$(QUIET)cp -r scripts/gen-pkg.py     	 aplus-$(PLATFORM)/utils
	$(QUIET)tar cJf aplus-$(PLATFORM).tar.xz aplus-$(PLATFORM) 
	$(QUIET)tar czf aplus-$(PLATFORM).tar.gz aplus-$(PLATFORM) 
	$(QUIET)zip     aplus-$(PLATFORM).zip    aplus-$(PLATFORM)
	$(QUIET)$(RM) -rf                        aplus-$(PLATFORM)


run: install
	$(QUIET)./scripts/run-$(VM) $(PLATFORM) $(VM_DEBUG)

run-live: install-live
	$(QUIET)./scripts/run-$(VM) $(PLATFORM) $(VM_DEBUG)

run-remote: install
	$(QUIET)./scripts/run-$(VM) --remote $(REMOTE) $(PLATFORM) $(VM_DEBUG)

run-live-remote: install-live
	$(QUIET)./scripts/run-$(VM) --remote $(REMOTE) $(PLATFORM) $(VM_DEBUG)

run-headless: install
	$(QUIET)./scripts/run-$(VM) --headless $(PLATFORM) $(VM_DEBUG)

run-live-headless: install-live
	$(QUIET)./scripts/run-$(VM) --headless $(PLATFORM) $(VM_DEBUG)

run-fast:
	$(QUIET)./scripts/run-$(VM) $(PLATFORM) $(VM_DEBUG)

run-fast-remote:
	$(QUIET)./scripts/run-$(VM) --remote $(REMOTE)$(PLATFORM) $(VM_DEBUG)

run-fast-headless:
	$(QUIET)./scripts/run-$(VM) --headless $(PLATFORM) $(VM_DEBUG)


clean: CLEANALL
distclean: clean DISTCLEANALL
	$(QUIET)$(RM) $(TARGET) config.mk config.mk.old config.h makew aplus-*.tar.gz aplus-*.tar.xz aplus-*.zip *.vmdk *.vdi *.vhd *.img
	$(QUIET)$(RM) -r docs/html docs/man sdk/toolchain
	$(QUIET)$(ROOTDIR)/scripts/get-pkg.py --clean 


.PHONY: docs distdocs report

docs:
	@doxygen docs/Doxyfile
distdocs: docs
	@tar cJf aplus-docs.tar.xz docs/man docs/html

report:
	$(QUIET)echo "    GEN     $(REPORT)"
	$(QUIET)./scripts/gen-report > $(REPORT)

-include build/build-format.mk
-include build/build-utils.mk