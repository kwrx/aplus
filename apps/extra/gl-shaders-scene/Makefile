INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include

LIBS      += ui GL stdc++ supc++ z m
RESOURCES += $(SYSROOT)/usr/share/applications/gl-shaders-scene.desktop

$(SYSROOT)/usr/share/applications/gl-shaders-scene.desktop: assets/gl-shaders-scene.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 -C $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
