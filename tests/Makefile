.PHONY: all install clean distclean


TESTS   := $(shell find * -maxdepth 1 -type d -exec basename {} \;)
TARGETS	:= $(shell find $(TESTS) -type f -iname "Makefile" -exec dirname {} \;)


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