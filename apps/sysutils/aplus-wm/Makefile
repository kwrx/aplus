INCLUDES += include
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog

LIBS     += ui ulog cairo pixman-1 freetype png webp z

RESOURCES 	+= $(SYSROOT)/etc/init.d/01-aplus-wm.sh

$(SYSROOT)/etc/init.d/01-aplus-wm.sh: assets/etc/init.d/01-aplus-wm.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
