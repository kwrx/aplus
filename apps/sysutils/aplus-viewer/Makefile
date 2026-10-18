INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2
CFLAGS += -DPATH_MAX=4096

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui

LIBS      += ui mupdf mupdf-third cairo pixman-1 freetype jpeg png z
RESOURCES += $(SYSROOT)/usr/share/applications/aplus-viewer.desktop

$(SYSROOT)/usr/share/applications/aplus-viewer.desktop: assets/aplus-viewer.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
