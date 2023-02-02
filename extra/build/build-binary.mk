.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d .toml

TARGET      ?= $(notdir $(shell pwd)).bin

CFLAGS	    ?= -include $(ROOTDIR)/config.h
CXXFLAGS    ?= -include $(ROOTDIR)/config.h
ASFLAGS	    ?=
LDFLAGS	    ?=

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?= c m gcc

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/bin
LIBDIR		?= $(SYSROOT)/usr/lib


include $(ROOTDIR)/extra/build/build-sources.mk
include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS) $(RESOURCES) $(CARGO_OBJS)
	$(QUIET)echo "    LD      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(LD) $(LDFLAGS) -Wl,-Map,$(TARGET).map -o $@ $(OBJS) $(CARGO_OBJS) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBS))
	

include $(ROOTDIR)/extra/build/build-objects.mk