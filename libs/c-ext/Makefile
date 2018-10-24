.PHONY: all
CC			?=
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT		:= 	$(SYSROOT)/$(TARGET)/lib/libc.a	\
				$(SYSROOT)/$(TARGET)/lib/libg.a

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
CPPFILES	:=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES		:=  $(CFILES:.c=.o) $(CXXFILES:.cpp=.o)

CFLAGS		:=  -pipe -I include -std=gnu99 -fpic -O3 -c -g
CPPFLAGS	:=  -pipe -I include -fpic -O3 -c -g


all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CP     " $(subst $(ROOT)/,,$@)
	@cp lib/$(@F) $@
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rvs $@ $(OFILES)


.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CPPFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT)