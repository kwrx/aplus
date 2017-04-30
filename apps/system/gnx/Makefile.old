.PHONY: all
OUTPUT		?=
CXX			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))

CXXFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
OFILES		:=  $(CXXFILES:.cpp=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)

CXXFLAGS	:=  -pipe -I include -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CXX) -pipe -O3 -s -o $@ $(OFILES) -lSDL2 -lSDL2_image -lSDL2_ttf -lfreetype -lpng -ljpeg -lz -lgcc


.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CXXFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $(subst $(ROOT)/,,$@)
	@$(ASM) $(NFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT)