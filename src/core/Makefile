
SRCDIRS	 := arch/$(PLATFORM) kernel
INCLUDES := ../include
LIBS	 := gcc

CFLAGS   := -include $(ROOTDIR)/config.h
ASFLAGS  := -include $(ROOTDIR)/config.h
DESTDIR  := $(SYSROOT)/boot


include ./arch/$(PLATFORM)/config.mk
include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build.mk