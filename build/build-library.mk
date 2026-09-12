.SUFFIXES: .c .cpp .cxx .cc .s .S .asm .h .hpp .d

TARGET      ?= lib$(notdir $(shell pwd)).a

CFLAGS	    ?=
CXXFLAGS    ?=
ASFLAGS	    ?=
ARFLAGS	    := -rcs

DEFINES     ?=
INCLUDES    ?= include
LIBS        ?= c m

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/lib



include $(ROOTDIR)/build/build-sources.mk
include $(ROOTDIR)/build/build-flags.mk

$(TARGET): $(OBJS) $(HDRS) $(RESOURCES)
	$(QUIET)echo "    AR      $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(RM) $@
	$(QUIET)$(AR) $(ARFLAGS) $@ $(OBJS)

include $(ROOTDIR)/build/build-objects.mk