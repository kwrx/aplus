
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


CFLAGS		+= -DDEBUG
CXXFLAGS	+=
LDFLAGS		+=
ARFLAGS		+= -rcs


CFILES		:= $(shell find $(SRCDIR)/* -type f -name "*.c")
OFILES		:= $(CFILES:.c=.o)

.PHONY: all clean install git dist distclean
all: $(OUTPUT)

$(OUTPUT): $(SRCDIR)/syscalls.c
	@echo "CC   " $(notdir $@.o)
	@$(CC) $(CFLAGS) -c $< -o $<.o
	@echo "AR   " $(notdir $@)
	@$(AR) $(ARFLAGS) $@ $(SRCDIR)/*.o


clean:
	-@$(RM) $(SRCDIR)/*.o
	-@$(RM) $(OUTPUT)


install: $(OUTPUT)
	@install -m 644 $(OUTPUT) $(SYSROOT)/$(PREFIX)/lib/$(OUTPUT)


distclean: clean
	@-$(RM) $(PACKAGE)-$(VERSION).tar.gz

dist: distclean
	@tar -cf $(PACKAGE)-$(VERSION).tar *
	@gzip $(PACKAGE)-$(VERSION).tar


