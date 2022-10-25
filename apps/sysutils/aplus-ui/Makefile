INCLUDES += include
INCLUDES += lib
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(SYSROOT)/usr/include
INCLUDES += $(SYSROOT)/usr/include/freetype2

LIBS     += cairo pixman-1 freetype webp png z
CFLAGS   += -include config.h
CXXFLAGS += -include config.h

include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-binary.mk