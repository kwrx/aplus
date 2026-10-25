DESTDIR     := $(SYSROOT)/usr/sbin
TARGET      := pymonitor.py
INSTALLED   := $(DESTDIR)/pymonitor

RESOURCES   += $(SYSROOT)/etc/init.d/03-pymonitor.sh

.PHONY: all install clean distclean

all: $(TARGET)

install: $(INSTALLED) $(RESOURCES)

$(INSTALLED): $(TARGET)
	$(QUIET)echo "    INSTALL $(TARGET) -> $(INSTALLED)"
	$(QUIET)mkdir -p $(DESTDIR)
	$(QUIET)install -m 755 $< $@

$(SYSROOT)/etc/init.d/03-pymonitor.sh: assets/etc/init.d/03-pymonitor.sh
	$(QUIET)echo "    INSTALL $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)mkdir -p $(dir $@)
	$(QUIET)install -m 755 $< $@

clean:

distclean:
	$(QUIET)$(RM) $(INSTALLED) $(RESOURCES)
