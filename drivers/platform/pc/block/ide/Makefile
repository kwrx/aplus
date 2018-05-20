
.PHONY: all
OUTPUT		?=
CC			?=
ROOT		?=
OUTPUT		:= $(ROOT)/root/boot/mods/$(notdir $(PWD)).ko

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)

CFLAGS		:=  -pipe -I $(ROOT)/kernel/include -I include -include $(ROOT)/config.h -O3  -std=gnu99 -nostdlib -masm=intel -c
SFLAGS		:=  -c -I $(ROOT)/kernel/include -I include -include config.h

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -nostdlib -O3 -r -o $@ $(OFILES)
	@echo "module /boot/mods/$(notdir $(subst $(ROOT)/,,$@))" >> $(ROOT)/root/boot/grub/grub.cfg


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