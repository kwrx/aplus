.PHONY: all
CC			?=
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT_A	:= $(SYSROOT)/$(TARGET)/lib/lib$(notdir $(PWD)).a
OUTPUT_SO	:= $(ROOT)/root/usr/lib/lib$(notdir $(PWD)).so

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
CPPFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES		:=  $(CFILES:.c=.o) $(CXXFILES:.cpp=.o)

CFLAGS		:=  -pipe -I include -std=gnu99 -fpic -O3 -c
CPPFLAGS	:=  -pipe -I include -fpic -O3 -c


all: $(OUTPUT_A) $(OUTPUT_SO)
$(OUTPUT_A): $(OFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(OFILES)

$(OUTPUT_SO): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(LD) -shared -o $@ $(OFILES)


.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CPPFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT_A) $(OUTPUT_SO)