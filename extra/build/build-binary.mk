.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= $(notdir $(shell pwd)).bin

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?= c m gcc

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/$(shell realpath --relative-base=$(ROOTDIR)/src/apps ..)



include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS)
	$(QUIET)echo "    LD      $@"
	$(QUIET)$(LD) $(LDFLAGS) -o $@ $(OBJS) $(addprefix -l,$(LIBS))

include $(ROOTDIR)/extra/build/build-objects.mk