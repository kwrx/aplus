CFLAGS      += -include $(ROOTDIR)/config.h
CFLAGS      += -isystem $(SYSROOT)/usr/include

RESOURCES   += $(SYSROOT)/usr/bin/reboot
RESOURCES   += $(SYSROOT)/usr/bin/halt

$(SYSROOT)/usr/bin/reboot: assets/usr/bin/reboot
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/usr/bin/halt: assets/usr/bin/halt
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
