.PHONY: all clean
.EXPORT_ALL_VARIABLES:

ARCH		:= i386
TARGET		:= i686-cross



include build/Makefile.tools
include build/Makefile.sources
include build/Makefile.flags

all:				\
	$(KERNEL_OUTPUT)	\
	KERNEL_MODULES		\
	$(KERNEL_ISO)
	@$(VMM)

$(KERNEL_OUTPUT): $(KERNEL_OBJECTS)
	@echo "  LD     " $@
	@$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS) $(LIBS)

KERNEL_MODULES:
	@echo "multiboot /x" > bin/boot/grub/grub.cfg
	@$(foreach dir, $(KERNEL_MODULES_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) CC=$(CC);)
	@echo "boot" >> bin/boot/grub/grub.cfg

$(KERNEL_ISO): $(KERNEL_OUTPUT) KERNEL_MODULES
	@echo "  ISO    " $@
	@echo "multiboot /x" >> bin/boot/grub/grub.cfg
	@echo "boot" >> bin/boot/grub/grub.cfg
	@grub-mkrescue -o $@ bin

.c.o:
	@echo "  CC     " $@
	@$(CC) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $@
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $@
	@$(ASM) $(NFLAGS) -o $@ $<

clean:
	@$(RM) $(KERNEL_OBJECTS) $(KERNEL_MODULES) $(KERNEL_ISO) $(KERNEL_OUTPUT)
	@$(RM) -r *.o

