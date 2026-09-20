INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui

LIBS      += ui GL stdc++ supc++ z m
RESOURCES += $(SYSROOT)/usr/share/applications/gl-gears.desktop

$(SYSROOT)/usr/share/applications/gl-gears.desktop: assets/gl-gears.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
