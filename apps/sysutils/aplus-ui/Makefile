INCLUDES += include
INCLUDES += lib
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(SYSROOT)/usr/include
INCLUDES += $(SYSROOT)/usr/include/freetype2

LIBS     += cairo pixman-1 freetype webp png z
CFLAGS   += -include config.h -Wno-parentheses
CXXFLAGS += -include config.h -Wno-parentheses

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk