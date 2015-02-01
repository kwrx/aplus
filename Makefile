
include Makefile.config


CFILES 	:= $(shell find src -type f -name "*.c")
CXXFILES:= $(shell find src -type f -name "*.cpp")
AFILES	:= $(shell find src -type f -name "*.asm" -path */$(ARCH)/*)
ASFILES	:= $(shell find src -type f -name "*.s" -path */$(ARCH)/*)
HFILES	:= $(shell find src -type f -name "*.h")

SFILES 	:= $(CFILES) $(CXXFILES) $(AFILES) $(ASFILES)
OFILES	:= $(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(AFILES:.asm=.o) $(ASFILES:.s=.o)


.PHONY: all clean git
.SUFFIXES: .asm

all: iso

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
	

iso: aplus
	@genisoimage -o $(TOP)/bin/initrd ramdisk
	@grub-mkrescue $(TOP)/bin -o $(TOP)/aplus.iso
	@$(RM) *.pcap
	@$(VMM)

clean:
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
	
	
