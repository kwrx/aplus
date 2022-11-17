.PHONY: all install clean distclean


DRIVERS := block char dev video tty virtio
DRIVERS += platform/pc

TARGETS	:= $(shell find $(DRIVERS) -type f -iname "Makefile" -exec dirname {} \;)



BUILDALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i           || exit 1; done
INSTALLALL: $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install   || exit 1; done
CLEANALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean     || exit 1; done
DISTCLEANALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i distclean || exit 1; done


all: BUILDALL
install: INSTALLALL
clean: CLEANALL
distclean: DISTCLEANALL