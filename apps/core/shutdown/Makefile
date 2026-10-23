DESTDIR 	:= $(SYSROOT)/usr/sbin
CFLAGS      += -include $(ROOTDIR)/config.h
CFLAGS      += -isystem $(SYSROOT)/usr/include

RESOURCES   += $(SYSROOT)/usr/sbin/reboot
RESOURCES   += $(SYSROOT)/usr/sbin/halt

$(SYSROOT)/usr/sbin/reboot: assets/usr/sbin/reboot
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/usr/sbin/halt: assets/usr/sbin/halt
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
