INCLUDES += include
INCLUDES += $(ROOTDIR)/include

RESOURCES += $(SYSROOT)/usr/include/aplus/ui.h

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk

# Defined after the includes on purpose: a rule placed above them would become the
# default goal and "make" in this directory would install the header instead of
# building the archive.
$(SYSROOT)/usr/include/aplus/ui.h: include/aplus/ui.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@
