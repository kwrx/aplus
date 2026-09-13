INCLUDES += include
INCLUDES += $(ROOTDIR)/include

# The sysroot headers arrive through -isystem rather than INCLUDES (which becomes -I):
# musl's endian.h trips -Wparentheses, and with -Werror on that turns a system header
# into a build failure as soon as its directory is named as a user include path.
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

RESOURCES += $(SYSROOT)/usr/include/aplus/ui.h
RESOURCES += $(SYSROOT)/usr/include/aplus/ui-widgets.h

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-library.mk

# Defined after the includes on purpose: a rule placed above them would become the
# default goal and "make" in this directory would install the header instead of
# building the archive.
$(SYSROOT)/usr/include/aplus/%.h: include/aplus/%.h
	$(QUIET)echo "    CP      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)cp -f $< $@
