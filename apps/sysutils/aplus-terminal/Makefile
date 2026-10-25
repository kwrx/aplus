INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog
LIBS    += ui ulog tsm cairo freetype pixman-1 png z

RESOURCES += $(SYSROOT)/usr/share/applications/aplus-terminal.desktop
RESOURCES += $(SYSROOT)/etc/init.d/01-aplus-terminal.sh

$(SYSROOT)/usr/share/applications/aplus-terminal.desktop: assets/aplus-terminal.desktop
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 644 $< $@

$(SYSROOT)/etc/init.d/01-aplus-terminal.sh: assets/etc/init.d/01-aplus-terminal.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -d $(@D)
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk
