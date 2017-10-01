.PHONY: all
OUTPUT		?=
CXX			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))

CXXFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES		:=  $(CXXFILES:.cpp=.o)

CXXFLAGS	:=  -pipe -I include -fpic -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -o $@ $(OFILES) -laplus -lcairo -lpixman-1 -lfreetype -lpng -lz -lstdc++ -lsupc++ -lgcc
	@$(RM) $(OFILES)


.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CXXFLAGS) -o $@ $<
	
clean:
	$(RM) $(OFILES) $(OUTPUT)