
include Makefile.in

ifeq ($(TARGET),)
CC			:= gcc
CXX			:= g++
LD			:= ld
AR			:= ar
else
CC			:= $(TARGET)-gcc
CXX			:= $(TARGET)-g++
LD			:= $(TARGET)-ld
AR			:= $(TARGET)-ar
endif


CFLAGS		+= -I $(SRCDIR)/include -I /opt/cross/usr/include/freetype2 -std=c99
CXXFLAGS	+=
LDFLAGS		+=
ARFLAGS		+= -rcs

HEADER		:= atk.h


CFILES		:= $(shell find $(SRCDIR)/* -type f -name "*.c")
OFILES		:= $(CFILES:.c=.o)

.PHONY: all clean install git dist distclean
all: $(OUTPUT)

$(OUTPUT) : $(OFILES)
	@echo "AR   " $(notdir $@)
	@$(AR) $(ARFLAGS) $@ $(OFILES)

.c.o:
	@echo "CC   " $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

test: $(TSTDIR)/main
$(TSTDIR)/main: $(TSTDIR)/main.c
	@echo "CC   " $(notdir $@)
	@$(CC) -Wall $< -o $@

clean:
	-@$(RM) $(OFILES)
	-@$(RM) -r $(SRCDIR)/*.o
	-@$(RM) -r $(TSTDIR)/*.o
	-@$(RM) $(TSTDIR)/main
	-@$(RM) $(OUTPUT)


install: $(OUTPUT)
	@install -m 644 $(SRCDIR)/include/$(HEADER) $(SYSROOT)/$(PREFIX)/include/$(HEADER)
	@install -m 644 $(OUTPUT) $(SYSROOT)/$(PREFIX)/lib/$(OUTPUT)
	@install -m 644 $(OUTPUT) $(SYSROOT)/$(PREFIX2)/lib/$(OUTPUT)


distclean: clean
	@-$(RM) $(PACKAGE)-$(VERSION).tar.gz

dist: distclean
	@tar -cf $(PACKAGE)-$(VERSION).tar *
	@gzip $(PACKAGE)-$(VERSION).tar

git:
	-@git add --all .
	-@git commit -m "$(COMMIT)"
	-@git push origin master
