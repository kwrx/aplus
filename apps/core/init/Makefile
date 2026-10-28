DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES 	+= $(ROOTDIR)/include
INCLUDES 	+= $(ROOTDIR)/lib/aplus/ulog/include
RESOURCES 	+= $(SYSROOT)/etc/init.sh
RESOURCES 	+= $(SYSROOT)/etc/init.d/00-init.sh

$(SYSROOT)/etc/init.sh: assets/etc/init.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/etc/init.d/00-init.sh: assets/etc/init.d/00-init.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(SYSROOT)/etc/init.d
	$(QUIET)install -m 644 $< $@


LDFLAGS 	+= -L$(ROOTDIR)/lib/aplus/ulog
LIBS 		+= ulog

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk