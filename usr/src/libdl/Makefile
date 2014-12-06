#Change it to cross compile
PLATFORM	:= posix


PREFIX		:= /usr
OUTPUT		:= libdl.a

SYSROOT		:= /opt/cross

PACKAGE		:= libdl
VERSION		:= 1.0.0

SRCDIR		:= src
TSTDIR		:= test

CC			:= i686-aplus-gcc
CXX			:= i686-aplus-g++
LD			:= @LD@
AR			:= i686-aplus-ar

WARNINGS	:= -Wall -Wno-return-type -Wno-implicit-int -Wno-implicit-function-declaration

CFLAGS		:= -O2 $(WARNINGS) 
CXXFLAGS	:= -O2 $(WARNINGS)
LDFLAGS		:=
ARFLAGS		:= -rcs

CFILES		:= $(shell find $(SRCDIR)/* -type f -name "*.c")
OFILES		:= $(CFILES:.c=.o)

.PHONY: all clean install git dist distclean
all: $(OUTPUT)

$(OUTPUT) : $(OFILES)
	@echo "AR\t" $(notdir $@)
	@$(AR) $(ARFLAGS) $@ $(OFILES)

.c.o:
	@echo "CC\t" $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@


test: $(TSTDIR)/main
$(TSTDIR)/main : $(TSTDIR)/main.c
	@echo "CC\t" $(notdir $<)
	@$(CC) -Wall -Wno-implicit-function-declaration $< -o $@ -ldl

clean:
	-@$(RM) $(SRCDIR)/*.o
	-@$(RM) $(TSTDIR)/*.o
	-@$(RM) $(TSTDIR)/main
	-@$(RM) $(OUTPUT)


install: $(OUTPUT)
	@install -m 644 $(OUTPUT) $(SYSROOT)/$(PREFIX)/lib/$(OUTPUT)


distclean: clean
	@-$(RM) $(PACKAGE)-$(VERSION).tar.gz
	@-$(RM) config.status
	@-$(RM) config.log
	@-$(RM) Makefile
	@-$(RM) -r autom4te.cache

dist: distclean
	@tar -cf $(PACKAGE)-$(VERSION).tar *
	@gzip $(PACKAGE)-$(VERSION).tar

git:
	-@git add --all .
	-@git commit -m "$(COMMIT)"
	-@git push origin master
