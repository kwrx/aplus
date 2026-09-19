DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES 	+= $(ROOTDIR)/include
RESOURCES 	+= $(SYSROOT)/etc/init.sh

$(SYSROOT)/etc/init.sh: scripts/init.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk