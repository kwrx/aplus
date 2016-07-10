
.PHONY: all
OUTPUT		?=
CXX			?=
ROOT		?=
OUTPUT		:= bin/usr/bin/$(notdir $(PWD))

CXXFILES	:=  $(shell $(FIND) * -type f -name "*.cpp")

all: $(OUTPUT)
$(OUTPUT): $(CFILES)
	@echo "  CXX    " $@
	@$(CXX) -pipe -I include -O3 -o $(ROOT)/$(OUTPUT) $(CXXFILES)
