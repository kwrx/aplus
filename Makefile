
include Makefile.config

# Kernel
CFILES 		:= $(shell find src -type f -name "*.c")
CXXFILES	:= $(shell find src -type f -name "*.cpp")
AFILES		:= $(shell find src -type f -name "*.asm" -path */$(ARCH)/*)
ASFILES		:= $(shell find src -type f -name "*.s" -path */$(ARCH)/*)
HFILES		:= $(shell find src -type f -name "*.h")

SFILES 		:= $(CFILES) $(CXXFILES) $(AFILES) $(ASFILES)
OFILES		:= $(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(AFILES:.asm=.o) $(ASFILES:.s=.o)


.PHONY: all clean git
.SUFFIXES: .asm

all: iso


ramdisk: aplus
	@-cd ramdisk/src && $(MAKE) && $(MAKE) install

modules: aplus
	@-cd modules && $(MAKE) clean && $(MAKE) && $(MAKE) install

ramdisk_clean:
	@-cd ramdisk/src && $(MAKE) clean

modules_clean:
	@-cd modules && $(MAKE) clean

aplus : $(OFILES)
	@echo "  LD      " $@
	@$(LD) $(LFLAGS) -o $@ $(OFILES) $(LIBS)
	@echo "  OBJCPY  " aplus.sym
	@$(OBJCPY) --only-keep-debug $@ aplus.sym
	@echo "  STRIP   " $@
	@$(OBJCPY) --strip-debug $@ $@
	@echo "  ZIP     " apluz
	@$(CP) $@ $(PREFIX)/$@
	@$(ZIP) $(PREFIX)/$@
	@$(MV) $(PREFIX)/$@.gz $(PREFIX)/apluz
	@echo "  IMG     " kernel.img
	@$(OBJCPY) $@ -O binary kernel.img

.c.o:
	@echo "  CC      " $<
	@$(CC) $(CFLAGS) $< -o $@

.cpp.o:
	@echo "  CXX     " $<
	@$(CXX) $(CXXFLAGS) $< -o $@

.asm.o:
	@echo "  ASM     " $<
	@$(ASM) $(AFLAGS) $< -o $@

.s.o:
	@echo "  AS      " $<
	@$(AS) $(ASFLAGS) $< -o $@
	

iso: ramdisk modules
	@genisoimage -o $(TOP)/bin/initrd ramdisk
	@grub-mkrescue $(TOP)/bin -o $(TOP)/aplus.iso
	@$(RM) *.pcap
	@$(VMM)

clean: ramdisk_clean modules_clean
	-@$(RM) $(OFILES)


doc:
	-@mkdir ./usr
	-@cp -r $(CROSSLIB)/src ./usr
	@doxygen docs/Doxyfile
	-@$(RM) -r usr
	
git: clean
	-@mkdir ./usr
	-@cp -r $(CROSSLIB)/src ./usr
	-@git add --all .
	-@git commit -m "$(COMMIT)"
	-@git push origin master
	-@$(RM) -r usr
	
	
