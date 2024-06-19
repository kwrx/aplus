.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d .rs .toml

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



include $(ROOTDIR)/build/build-sources.mk
include $(ROOTDIR)/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS) $(RESOURCES)
	$(QUIET)echo "    LD      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(LD) -r $(LDFLAGS) -o $@ $(OBJS) $(addprefix -l,$(LIBS))

include $(ROOTDIR)/build/build-objects.mk