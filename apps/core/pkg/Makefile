CFLAGS      += -include $(ROOTDIR)/config.h
CFLAGS      += -isystem $(SYSROOT)/usr/include

LIBS        += sqlite3 curl ssl crypto lzma z m

RESOURCES   += $(SYSROOT)/etc/pkg.conf
RESOURCES   += $(SYSROOT)/var/lib/pkg
RESOURCES   += $(SYSROOT)/var/cache/pkg

$(SYSROOT)/etc/pkg.conf: assets/etc/pkg.conf
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 644 $< $@

$(SYSROOT)/var/lib/pkg $(SYSROOT)/var/cache/pkg:
	$(QUIET)echo "    MKDIR   $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d -m 755 $@

schema.o: schema.sql

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
