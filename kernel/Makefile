
SRCDIRS	 := $(ROOTDIR)/arch/$(PLATFORM) $(ROOTDIR)/libk .
INCLUDES := $(ROOTDIR)/include
DEFINES	 := KERNEL=1
LIBS	 := gcc

CFLAGS   := -include $(ROOTDIR)/config.h -fno-builtin
CXXFLAGS := -include $(ROOTDIR)/config.h -fno-builtin
ASFLAGS  := -include $(ROOTDIR)/config.h
DESTDIR  := $(SYSROOT)/boot
TARGET	 := aplus.bin


include $(ROOTDIR)/arch/$(PLATFORM)/config.mk
include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-binary.mk
