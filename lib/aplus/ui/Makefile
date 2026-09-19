INCLUDES += include
INCLUDES += $(ROOTDIR)/include

CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

RESOURCES += $(SYSROOT)/usr/include/aplus/ui.h
RESOURCES += $(SYSROOT)/usr/include/aplus/ui-widgets.h

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk

$(SYSROOT)/usr/include/aplus/%.h: include/aplus/%.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@
