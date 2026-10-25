INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2
CFLAGS += -DPATH_MAX=4096
CFLAGS += -DNAME_MAX=255

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog

LIBS      += ui ulog cairo pixman-1 freetype png z
RESOURCES += $(SYSROOT)/usr/share/applications/aplus-explorer.desktop

$(SYSROOT)/usr/share/applications/aplus-explorer.desktop: assets/aplus-explorer.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
