.PHONY: all
CC			?=
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT_A	:= $(SYSROOT)/$(TARGET)/lib/lib$(notdir $(PWD)).a
OUTPUT_SO	:= $(ROOT)/root/usr/lib/lib$(notdir $(PWD)).so

OUTPUT_H	:=  $(SYSROOT)/$(TARGET)/include/vterm.h 				\
				$(SYSROOT)/$(TARGET)/include/vterm_keycodes.h		\
				$(ROOT)/root/usr/include/vterm.h					\
				$(ROOT)/root/usr/include/vterm_keycodes.h


CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
CPPFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
HFILES		:=  $(shell $(FIND) $(PWD)/include/* -type f -name "*.h")
OFILES		:=  $(CFILES:.c=.o) $(CXXFILES:.cpp=.o)

CFLAGS		:=  -pipe -I include -std=gnu99 -fpic -O3 -c -g
CPPFLAGS	:=  -pipe -I include -fpic -O3 -c -g


all: $(OUTPUT_A) $(OUTPUT_SO) $(OUTPUT_H)
$(OUTPUT_A): $(OFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(OFILES)

$(OUTPUT_SO): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(LD) -shared -o $@ $(OFILES)

$(OUTPUT_H): $(HFILES)
	@echo "  CP     " $(subst $(ROOT)/,,$@)
	@cp $(<D)/$(@F) $@

.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CPPFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT_A) $(OUTPUT_SO)