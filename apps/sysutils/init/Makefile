DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES 	+= $(ROOTDIR)/include
RESOURCES 	+= $(SYSROOT)/etc/init.sh

$(SYSROOT)/etc/init.sh: scripts/init.sh
	echo "    CP      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)cp -f $< $@

include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-binary.mk