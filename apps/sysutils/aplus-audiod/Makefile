INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/audio/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog

LIBS      += ulog
RESOURCES += $(SYSROOT)/etc/init.d/01-aplus-audiod.sh

$(SYSROOT)/etc/init.d/01-aplus-audiod.sh: assets/etc/init.d/01-aplus-audiod.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
