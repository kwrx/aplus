INCLUDES += include
INCLUDES += $(ROOTDIR)/include

CFLAGS   += -isystem $(SYSROOT)/usr/include

RESOURCES += $(SYSROOT)/usr/include/aplus/ulog.h

$(SYSROOT)/usr/include/aplus/ulog.h: include/aplus/ulog.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk
