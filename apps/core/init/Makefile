DESTDIR 	:= $(SYSROOT)/usr/sbin
INCLUDES 	+= $(ROOTDIR)/include
INCLUDES 	+= $(ROOTDIR)/lib/aplus/ulog/include
RESOURCES 	+= $(SYSROOT)/etc/init.sh

$(SYSROOT)/etc/init.sh: assets/etc/init.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

LDFLAGS 	+= -L$(ROOTDIR)/lib/aplus/ulog
LIBS 		+= ulog

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk