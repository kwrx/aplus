.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d .rs .toml

TARGET      ?= $(notdir $(shell pwd)).ko

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?=

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/lib/modules/$(shell realpath --relative-base=$(ROOTDIR)/drivers ..)



include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS) $(RESOURCES)
	$(QUIET)echo "    LD      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(LD) -r $(LDFLAGS) -o $@ $(OBJS)

include $(ROOTDIR)/extra/build/build-objects.mk