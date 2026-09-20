INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/cairo-ext/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2
CFLAGS += -DPATH_MAX=4096

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/cairo-ext

LIBS      += ui cairo-ext cairo pixman-1 freetype webp jpeg png z
RESOURCES += $(SYSROOT)/usr/share/applications/aplus-image-viewer.desktop

$(SYSROOT)/usr/share/applications/aplus-image-viewer.desktop: assets/aplus-image-viewer.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
