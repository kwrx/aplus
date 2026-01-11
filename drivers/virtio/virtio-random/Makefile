
SRCDIRS	 := .
INCLUDES := $(ROOTDIR)/include
DEFINES	 := KERNEL=1 MODULE=1
LIBS	 := 

CFLAGS   := -include $(ROOTDIR)/config.h
ASFLAGS  := -include $(ROOTDIR)/config.h


include $(ROOTDIR)/drivers/platform/$(PLATFORM)/config.mk
include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-ko.mk