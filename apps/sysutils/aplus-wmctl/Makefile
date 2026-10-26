INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui

LIBS += ui cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
