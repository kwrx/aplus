.PHONY: all
CC			?=
CXX			?=
ROOT		?=
SYSROOT		?=
TARGET		?=

OUTPUT_A	:= $(SYSROOT)/$(TARGET)/lib/lib$(notdir $(PWD)).a
OUTPUT_SO	:= $(ROOT)/root/usr/lib/lib$(notdir $(PWD)).so

CFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
SFILES		:=  $(shell $(FIND) $(PWD)/* -type f -name "*.S")
OFILES		:=  $(CFILES:.c=.o) $(SFILES:.S=.o)

DEFS		:= 	-DHAVE_FCNTL -D_NO_POSIX_SPAWN -D_GNU_SOURCE				\
				-D_I386MACH_NEED_SOTYPE_FUNCTION -DMISSING_SYSCALL_NAMES	\
				-DINTERNAL_NEWLIB -DHAVE_SYSTEM


CFLAGS		:=  -pipe -I include -std=gnu99 -fpic -fno-builtin -O2 -fstack-protector-strong -c -g $(DEFS)
SFLAGS		:=  -pipe -fpic -fno-builtin -O2 -c -g $(DEFS)


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

.S.o:
	@echo "  AS     " $(subst $(ROOT)/,,$@)
	@$(CC) $(SFLAGS) -o $@ $<
	
	
clean:
	$(RM) $(OFILES) $(OUTPUT_A) $(OUTPUT_SO)