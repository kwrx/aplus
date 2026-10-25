INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2
CFLAGS += -DPATH_MAX=4096

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog

LIBS += ui ulog cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk

ifeq ($(CONFIG_HAVE_AI),y)
	LIBS += curl ssl crypto z
endif

include $(ROOTDIR)/build/build-binary.mk
