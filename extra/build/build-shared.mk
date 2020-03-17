.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= $(notdir $(shell pwd)).so

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?= c m

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/lib



include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS)
	$(QUIET)$(LD) -r $(LDFLAGS) -o $@ $(OBJS) $(addprefix -l,$(LIBS))

include $(ROOTDIR)/extra/build/build-objects.mk