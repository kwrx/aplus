INCLUDES += include
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include


CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

LIBS     += ui cairo pixman-1 freetype png webp z

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
