.SUFFIXES: .toml
.PHONY: Cargo.toml

TARGET      ?= lib$(notdir $(shell pwd)).a

SRCDIRS     ?= .
DESTDIR     ?= $(SYSROOT)/usr/lib


include $(ROOTDIR)/extra/build/build-flags.mk

$(TARGET): Cargo.toml
	$(QUIET)echo "    CARGO   $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CARGO) xbuild $(CARGO_FLAGS)
	$(QUIET)$(CP) target/$(CARGO_TARGET)/$(CARGO_BUILD)/$(notdir $(TARGET)) $@
	$(QUIET)$(CP) target/$(CARGO_TARGET)/$(CARGO_BUILD)/$(notdir $(shell basename $(TARGET) .a)).rlib $(shell basename $@ .a).rlib

include $(ROOTDIR)/extra/build/build-objects.mk