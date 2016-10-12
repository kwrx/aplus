.PHONY: all
OUTPUT		?=
CC			?=
AR			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))
LIBRARY		:= $(ROOT)/bin/usr/lib/lib$(notdir $(PWD)).a

CXXFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
LCXXFILES	:=  $(shell $(FIND) $(PWD)/lib/* -type f -name "*.cpp")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CXXFILES:.cpp=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)
LFILES		:=  $(LCXXFILES:.cpp=.o)

CFLAGS		:=  -pipe -I include -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT) $(LIBRARY)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CXX) -pipe -O3 -s -o $@ $(OFILES) \
		-lcairo -lpixman-1 -lfreetype -lpng -ljpeg -lz -lgcc

$(LIBRARY): $(LFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(LFILES)

.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $(subst $(ROOT)/,,$@)
	@$(ASM) $(NFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT) $(LIBRARY)