.PHONY: all
OUTPUT		?=
CC			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CFILES:.cpp=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)

CFLAGS		:=  -pipe -I include -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CXX) -pipe -O3 -s -o $@ $(OFILES) -lgnx -lcairo -lfontconfig -lexpat -lfreetype -lpixman-1 -lpng -lz -lgcc


.cpp.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $(subst $(ROOT)/,,$@)
	@$(ASM) $(NFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT)