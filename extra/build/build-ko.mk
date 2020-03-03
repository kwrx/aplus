.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= $(notdir $(shell pwd)).ko

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?=

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/lib/modules/$(shell realpath --relative-base=$(ROOTDIR)/src/initrd ..)



include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS)
	$(QUIET)echo "    LD      $@"
	$(QUIET)$(LD) -r $(LDFLAGS) -o $@ $(OBJS)

include $(ROOTDIR)/extra/build/build-objects.mk