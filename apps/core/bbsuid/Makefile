DESTDIR      := $(SYSROOT)/usr/sbin
INSTALL_MODE := 4755

LINKS        := su passwd

include $(ROOTDIR)/build/cross.mk
include $(ROOTDIR)/build/build-binary.mk

.PHONY: links unlinks

install: links
distclean: unlinks

links:
	$(QUIET)mkdir -p $(SYSROOT)/usr/bin
	$(QUIET)for i in $(LINKS); do                                \
		echo "    LN      usr/bin/$$i -> ../sbin/bbsuid";        \
		ln -sfn ../sbin/bbsuid $(SYSROOT)/usr/bin/$$i || exit 1; \
	done

unlinks:
	$(QUIET)for i in $(LINKS); do $(RM) $(SYSROOT)/usr/bin/$$i; done
