.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= $(notdir $(shell pwd)).bin

CFLAGS	    ?= -std=c17   -include $(ROOTDIR)/config.h
CXXFLAGS    ?= -std=c++17 -include $(ROOTDIR)/config.h
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

$(TARGET): $(OBJS) $(HDRS)
	$(QUIET)echo "    LD      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(LD) $(LDFLAGS) -Wl,-Map,$(TARGET).map -o $@ $(OBJS) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBS))
	

include $(ROOTDIR)/extra/build/build-objects.mk