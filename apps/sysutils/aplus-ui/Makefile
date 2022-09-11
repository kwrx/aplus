INCLUDES += include
INCLUDES += lib
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(SYSROOT)/usr/include

LIBS     += cairo pixman-1 stdc++ supc++
CFLAGS   += -include config.h
CXXFLAGS += -include config.h

include $(ROOTDIR)/extra/build/cross.mk
include $(ROOTDIR)/extra/build/build-binary.mk