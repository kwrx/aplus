.PHONY: all

CC			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/sbin/$(notdir $(PWD))

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)

CFLAGS		:=  -pipe -I include -std=gnu99 -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -s -o $@ $(OFILES) -lgcc


.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $(subst $(ROOT)/,,$@)
	@$(ASM) $(NFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT)