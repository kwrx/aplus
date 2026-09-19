INCLUDES += include
INCLUDES += $(ROOTDIR)/include

CFLAGS   += -isystem $(SYSROOT)/usr/include

RESOURCES += $(SYSROOT)/usr/include/aplus/cairo-ext/cairo-webp.h
RESOURCES += $(SYSROOT)/usr/include/aplus/cairo-ext/cairo-jpeg.h

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk

$(SYSROOT)/usr/include/aplus/cairo-ext/cairo-webp.h: include/aplus/cairo-ext/cairo-webp.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@

$(SYSROOT)/usr/include/aplus/cairo-ext/cairo-jpeg.h: include/aplus/cairo-ext/cairo-jpeg.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@
