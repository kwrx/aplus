
.PHONY: all
OUTPUT		?=
CC		?=
ROOT		?=
OUTPUT		:= bin/boot/mods/$(notdir $(PWD)).ko

CFILES		:=  $(shell $(FIND) * -type f -name "*.c")

all: $(OUTPUT)
$(OUTPUT): $(CFILES)
	@echo "  CC     " $@
	@$(CC) -pipe -I $(ROOT)/src/include -I include -include $(ROOT)/config.h \
		-nostdlib -std=c99 -O3 -r -o $(ROOT)/$(OUTPUT) $(CFILES) -lgcc
	@echo "module /boot/mods/$(notdir $(OUTPUT))" >> $(ROOT)/bin/boot/grub/grub.cfg
