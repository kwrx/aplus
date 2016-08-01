.PHONY: all
.SUFFIXES: .java .class .asm

OUTPUT		?=
CC			?=
AR			?=
ROOT		?=

OUTPUT_BIN	:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))
OUTPUT_LIB	:= $(ROOT)/bin/usr/lib/lib$(notdir $(PWD)).a
OUTPUT_INC	:= $(ROOT)/bin/usr/include/avm.h
OUTPUT_RT	:= $(ROOT)/bin/usr/lib/avm/rt.jar

CFILES		:=  $(shell $(FIND) $(PWD)/src/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/src/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/src/* -type f -name "*.asm")
JFILES		:=  $(shell $(FIND) $(PWD)/lib/* -type f -name "*.java")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)
CLASSFILES	:=	$(JFILES:.java=.class)

CFLAGS		:=  -pipe -I src/include -include config.h -std=c99 -O3 -c
SFLAGS		:=  -c -I include

all: $(OUTPUT_BIN) $(OUTPUT_LIB) $(OUTPUT_INC) $(OUTPUT_RT)
$(OUTPUT_BIN): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -s -o $@ $(OFILES) -lc -lm -lgcc

$(OUTPUT_LIB): $(OFILES)
	@echo "  AR     " $(subst $(ROOT)/,,$@)
	@$(AR) -rcs $@ $(filter-out $(PWD)/src/main.o,$(OFILES))
	
$(OUTPUT_RT): $(CLASSFILES)
	@echo "  MKDIR  " $(subst $(ROOT)/,,$(dir $(OUTPUT_RT)))
	@mkdir -p $(dir $(OUTPUT_RT))
	@echo "  JAR    " $(subst $(ROOT)/,,$@)
	@cd $(PWD)/lib && jar -cf $(OUTPUT_RT) $(subst $(PWD)/lib/,,$(CLASSFILES))
	
$(OUTPUT_INC):
	@echo "  COPY   " $(subst $(ROOT)/,,$@)
	@install -D -m 644 -t $(dir $(OUTPUT_INC)) src/include/avm.h

.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $(subst $(ROOT)/,,$@)
	@$(ASM) $(NFLAGS) -o $@ $<
	
.java.class:
	@echo "  JAVA   " $(subst $(ROOT)/,,$@)
	@javac -classpath lib $<
	
clean:
	$(RM) $(OFILES) $(CLASSFILES) $(OUTPUT_BIN) $(OUTPUT_LIB) $(OUTPUT_RT)