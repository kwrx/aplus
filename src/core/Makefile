
SRCDIRS	 := arch/$(PLATFORM) kernel libk
INCLUDES := ../include
DEFINES	 := KERNEL=1
LIBS	 := gcc

CFLAGS   := -include $(ROOTDIR)/config.h -fno-builtin
ASFLAGS  := -include $(ROOTDIR)/config.h
DESTDIR  := $(SYSROOT)/boot


include ./arch/$(PLATFORM)/config.mk
include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-binary.mk
