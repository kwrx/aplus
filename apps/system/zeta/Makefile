.PHONY: all
.SUFFIXES: .java .class .asm

CC			?=
ROOT		?=
OUTPUT		:= $(ROOT)/bin/usr/bin/$(notdir $(PWD))
OUTPUT_RT	:= $(ROOT)/bin/usr/lib/avm/zeta.jar

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.s")
AFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.asm")
JFILES		:=  $(shell $(FIND) $(PWD)/lib/java/* -type f -name "*.java")
OFILES		:=  $(CFILES:.c=.o) $(AFILES:.asm=.o) $(SFILES:.s:.o)
CLASSFILES	:=	$(JFILES:.java=.class)

CFLAGS		:=  -pipe -I include -I $(ROOT)/bin/usr/include -std=c99 -O3 -c
SFLAGS		:=  -c -I include 

all: $(OUTPUT) $(OUTPUT_RT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -s -o $@ $(OFILES) -L $(ROOT)/bin/usr/lib -lavm -lffi -lzip -lz -lc -lm -lgcc

$(OUTPUT_RT): $(CLASSFILES)
	@echo "  MKDIR  " $(subst $(ROOT)/,,$(dir $(OUTPUT_RT)))
	@mkdir -p $(dir $(OUTPUT_RT))
	@echo "  JAR    " $(subst $(ROOT)/,,$@)
	@cd $(PWD)/lib/java && jar -cf $(OUTPUT_RT) $(subst $(PWD)/lib/java/,,$(CLASSFILES))

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
	@javac -classpath lib/java $<
	
clean:
	$(RM) $(OFILES) $(OUTPUT) $(CLASSFILES) $(OUTPUT_RT)