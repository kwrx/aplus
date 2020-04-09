.PHONY: all install clean distclean


DRIVERS := block char dev video virtio
DRIVERS += platform/pc

TARGETS	:= $(shell find $(DRIVERS) -type f -iname "Makefile" -exec dirname {} \;)



BUILDALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i           || break; done
INSTALLALL: $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i install   || break; done
CLEANALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i clean     || break; done
DISTCLEANALL:   $(TARGETS)
	$(QUIET)for i in $^; do $(MAKE) -C $$i distclean || break; done


all: BUILDALL
install: INSTALLALL
clean: CLEANALL
distclean: DISTCLEANALL