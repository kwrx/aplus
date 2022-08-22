.PHONY: all install clean distclean


APPS    := coreutils sysutils extra test
TARGETS	:= $(shell find $(APPS) -type f -iname "Makefile" -exec dirname {} \;)



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