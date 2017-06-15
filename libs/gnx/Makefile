.PHONY: all
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT_A	:= $(SYSROOT)/$(TARGET)/lib/lib$(notdir $(PWD)).a
OUTPUT_SO	:= $(ROOT)/bin/usr/lib/lib$(notdir $(PWD)).so

CXXFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES		:=  $(CXXFILES:.cpp=.o)

CXXFLAGS	:=  -pipe -I include -O3 -c


all: $(OUTPUT_A)
$(OUTPUT_A): $(OFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(OFILES)



.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CXXFLAGS) -o $@ $<

	
clean:
	$(RM) $(OFILES) $(OUTPUT_A)