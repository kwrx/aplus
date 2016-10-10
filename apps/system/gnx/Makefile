.PHONY: all
OUTPUT		?=
CC			?=
AR			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))
LIBRARY		:= $(ROOT)/bin/usr/lib/lib$(notdir $(PWD)).a

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
LCFILES		:=  $(shell $(FIND) $(PWD)/lib/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)
LFILES		:=  $(LCFILES:.c=.o)

CFLAGS		:=  -pipe -I include -std=c99 -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT) $(LIBRARY)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -Ofast -s -o $@ $(OFILES) \
		-lSDL2 -lSDL2_ttf -lSDL2_image -lfreetype -lpng -ljpeg -lz -lgcc

$(LIBRARY): $(LFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(LFILES)

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
	$(RM) $(OFILES) $(OUTPUT) $(LIBRARY)