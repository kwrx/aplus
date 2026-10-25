DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES    += $(ROOTDIR)/lib/aplus/ulog/include

RESOURCES   += $(SYSROOT)/etc/init.d/02-crond.sh
RESOURCES   += $(SYSROOT)/etc/crontab

LDFLAGS     += -L$(ROOTDIR)/lib/aplus/ulog
LIBS        += ulog

$(SYSROOT)/etc/init.d/02-crond.sh: assets/etc/init.d/02-crond.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/etc/crontab: assets/etc/crontab
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
