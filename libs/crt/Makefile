.PHONY: all
CC			?=
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT		:=  $(SYSROOT)/$(TARGET)/lib/crt0.o		\
				$(SYSROOT)/$(TARGET)/lib/crti.o		\
				$(SYSROOT)/$(TARGET)/lib/crtn.o		\
				$(SYSROOT)/$(TARGET)/lib/rcrt1.o 	\
				$(SYSROOT)/$(TARGET)/lib/Scrt1.o

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
OFILES		:=  $(CFILES:.c=.o) $(SFILES:.s=.o)

CFLAGS		:=  -pipe -I include -std=gnu99 -fpic -O3 -c -g
SFLAGS		:=  -pipe -fpic -O3 -c -g


all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CP     " $(subst $(ROOT)/,,$@)
	@cp $(<D)/$(@F) $@
	

.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(CC) $(SFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT)