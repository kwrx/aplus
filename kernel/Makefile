
SRCDIRS	  := $(ROOTDIR)/arch/$(PLATFORM) $(ROOTDIR)/libk .
INCLUDES  := $(ROOTDIR)/include
DEFINES	  := KERNEL=1
LIBS	  := gcc

CFLAGS    := -include $(ROOTDIR)/config.h -fno-builtin
CXXFLAGS  := -include $(ROOTDIR)/config.h -fno-builtin
ASFLAGS   := -include $(ROOTDIR)/config.h

DESTDIR   := $(SYSROOT)/boot
TARGET	  := aplus.bin

LDFLAGS   := -Wl,--whole-archive $(ROOTDIR)/kernel-rust/libkernel-rust.a -Wl,--no-whole-archive

RESOURCES := $(ROOTDIR)/kernel-rust/libkernel-rust.a

include $(ROOTDIR)/arch/$(PLATFORM)/config.mk
include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
