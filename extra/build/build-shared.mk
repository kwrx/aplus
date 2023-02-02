.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d .toml

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

$(TARGET): $(OBJS) $(HDRS) $(RESOURCES) $(CARGO_OBJS)
	$(QUIET)echo "    LD      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(LD) -r $(LDFLAGS) -o $@ $(OBJS) $(CARGO_OBJS) $(addprefix -l,$(LIBS))

include $(ROOTDIR)/extra/build/build-objects.mk