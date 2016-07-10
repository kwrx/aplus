
.PHONY: all
OUTPUT		?=
CC			?=
ROOT		?=
OUTPUT		:= bin/usr/bin/$(notdir $(PWD))

CFILES		:=  $(shell $(FIND) * -type f -name "*.c")

all: $(OUTPUT)
$(OUTPUT): $(CFILES)
	@echo "  CC     " $@
	@$(CC) -pipe -I include -std=c99 -O3 -o $(ROOT)/$(OUTPUT) $(CFILES)
