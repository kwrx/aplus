
SRCDIRS	 := .
INCLUDES := $(ROOTDIR)/include
DEFINES	 := KERNEL=1 MODULE=1
LIBS	 := 

CFLAGS   := -include $(ROOTDIR)/config.h
ASFLAGS  := -include $(ROOTDIR)/config.h


include $(ROOTDIR)/drivers/platform/$(PLATFORM)/config.mk
include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-ko.mk