.PHONY: all
OUTPUT		?=
CXX			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
CXXFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES		:=  $(CFILES:.c=.o) $(CXXFILES:.cpp=.o)

CFLAGS		:=  -pipe -I include -fpic -O3 -c -std=gnu99
CXXFLAGS	:=  -pipe -I include -fpic -O3 -c -std=gnu++14


all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -o $@ $(OFILES) -laplus -lcairo -lpixman-1 -lfreetype -lwebp -lpng -ltiff -ljpeg -lz -lstdc++ -lsupc++ -lgcc
	@$(RM) $(OFILES)


.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CXXFLAGS) -o $@ $<

.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<
	
clean:
	$(RM) $(OFILES) $(OUTPUT)