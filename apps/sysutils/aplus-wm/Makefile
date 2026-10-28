INCLUDES += include
INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS   += -include $(ROOTDIR)/config.h
CFLAGS   += -isystem $(SYSROOT)/usr/include
CFLAGS   += -isystem $(SYSROOT)/usr/include/freetype2

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog

LIBS     += ui ulog cairo pixman-1 freetype png webp z

RESOURCES 	+= $(SYSROOT)/etc/init.d/01-aplus-wm.sh
RESOURCES 	+= $(patsubst assets/%,$(SYSROOT)/%,$(wildcard assets/usr/share/themes/*.conf))
RESOURCES 	+= $(patsubst assets/%,$(SYSROOT)/%,$(wildcard assets/usr/share/images/*.webp))
RESOURCES 	+= $(SYSROOT)/etc/ui/theme.conf

$(SYSROOT)/etc/init.d/01-aplus-wm.sh: assets/etc/init.d/01-aplus-wm.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/usr/share/themes/%.conf: assets/usr/share/themes/%.conf
	$(QUIET)echo "    INSTALL $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -D -m 644 $< $@

$(SYSROOT)/usr/share/images/%.webp: assets/usr/share/images/%.webp
	$(QUIET)echo "    INSTALL $(shell realpath -m --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -D -m 644 $< $@

$(SYSROOT)/etc/ui/theme.conf: | $(SYSROOT)/usr/share/themes/default.conf
	$(QUIET)echo "    LN      $(shell realpath -m -s --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)ln -sfn ../../usr/share/themes/default.conf $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
