INCLUDES += $(ROOTDIR)/include
INCLUDES += $(ROOTDIR)/lib/aplus/ui/include
INCLUDES += $(ROOTDIR)/lib/aplus/ulog/include
INCLUDES += $(ROOTDIR)/lib/aplus/ai/include

CFLAGS += -include $(ROOTDIR)/config.h
CFLAGS += -isystem $(SYSROOT)/usr/include
CFLAGS += -isystem $(SYSROOT)/usr/include/freetype2
CFLAGS += -DPATH_MAX=4096

LDFLAGS += -L$(ROOTDIR)/lib/aplus/ui
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ulog
LDFLAGS += -L$(ROOTDIR)/lib/aplus/ai

LIBS += ui ulog cairo pixman-1 freetype png z

include $(ROOTDIR)/build/cross.mk

ifeq ($(CONFIG_HAVE_AI),y)
	LIBS += ai curl ssl crypto z
	RESOURCES += $(SYSROOT)/etc/init.d/03-aplus-launcher.sh
endif

$(SYSROOT)/etc/init.d/03-aplus-launcher.sh: assets/etc/init.d/03-aplus-launcher.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)install -m 755 $< $@

include $(ROOTDIR)/build/build-binary.mk
