INCLUDES += include
INCLUDES += $(ROOTDIR)/include

CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

RESOURCES += $(SYSROOT)/usr/include/aplus/ui.h
RESOURCES += $(SYSROOT)/usr/include/aplus/ui-widgets.h
RESOURCES += $(SYSROOT)/usr/include/aplus/ui-draw.h
RESOURCES += $(SYSROOT)/usr/include/aplus/ui-keymap.h

# The default icon theme, rendered from assets/icons/svg by scripts/gen-icons and installed
# as it stands: nothing here needs an SVG renderer at build time.
RESOURCES += $(patsubst assets/icons/%,$(SYSROOT)/usr/share/icons/%,$(wildcard assets/icons/*/*/*.png))

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk

$(SYSROOT)/usr/include/aplus/%.h: include/aplus/%.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@

$(SYSROOT)/usr/share/icons/%.png: assets/icons/%.png
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@
