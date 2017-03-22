.PHONY: all
CC			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT_SO	:= $(ROOT)/bin/usr/lib/lib$(notdir $(PWD)).so

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)

CFLAGS		:=  -pipe -I include -std=c99 -O3 -c
SFLAGS		:=  -c -I include



all: $(OUTPUT_SO)
$(OUTPUT_SO): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(LD) -r -o $@ $(OFILES)


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
	$(RM) $(OFILES) $(OUTPUT_A) $(OUTPUT_SO)