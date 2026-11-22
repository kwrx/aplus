INCLUDES    += $(ROOTDIR)/lib/aplus/ai/include

CFLAGS      += -include $(ROOTDIR)/config.h
CFLAGS      += -isystem $(SYSROOT)/usr/include

LDFLAGS     += -L$(ROOTDIR)/lib/aplus/ai

include $(ROOTDIR)/build/cross.mk

ifeq ($(CONFIG_HAVE_AI),y)
	LIBS    += ai curl ssl crypto z
endif

include $(ROOTDIR)/build/build-binary.mk
