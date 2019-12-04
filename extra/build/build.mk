.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= $(notdir $(shell pwd))

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?= c m

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/bin



include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS)
	$(QUIET)echo "    LD      $@"
	$(QUIET)$(LD) $(LDFLAGS) -o $@ $(OBJS) $(addprefix -l,$(LIBS))

include $(ROOTDIR)/extra/build/build-objects.mk