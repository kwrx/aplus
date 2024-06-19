DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES 	+= $(ROOTDIR)/include
RESOURCES 	+= $(SYSROOT)/etc/init.sh

$(SYSROOT)/etc/init.sh: scripts/init.sh
	echo "    CP      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)cp -f $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk