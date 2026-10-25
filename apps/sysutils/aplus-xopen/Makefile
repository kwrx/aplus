INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog
LIBS    += ulog

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
