

#
#  Author:
#       Antonio Natale <inferdevil97@gmail.com>
#
#  Copyright (c) 2014 WareX
#
#  This program is kfree software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the kfree Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

PRJDIR 	:= $(PWD)
SRCDIR 	:= $(PRJDIR)/src
TLSDIR 	:= $(PRJDIR)/bin/tools
BINDIR 	:= $(PRJDIR)/bin
INCDIR 	:= $(PRJDIR)/src/include
USRDIR 	:= $(PRJDIR)/usr
IRDDIR	:= $(PRJDIR)/initrd

TARGET 	:= i586-elf
PREFIX 	:= $(BINDIR)

CC	:= $(TARGET)-gcc
CXX	:= $(TARGET)-g++
LD	:= $(TARGET)-ld
ASM	:= nasm

CP	:= cp
MV	:= mv
MKDIR	:= mkdir

MKISO	:= grub-mkrescue
MKIRD	:= $(TLSDIR)/mkinitrd/mkinitrd
VM	:= qemu

ARCH	:= X86
DEFINES	:= -D$(ARCH) -DDEBUG -DAPLUS -DVIDEOMODE
LIBS	:= -lx86 -lm -lc -lgcc

CFLAGS	:= $(DEFINES) -I $(INCDIR) -w -c -masm=intel -ffreestanding -fno-builtin -std=c99
CXXFLAGS:= $(DEFINES) -I $(INCDIR) -w -c -masm=intel -ffreestanding -fno-builtin -fno-rtti -fno-exceptions -fpermissive
AFLAGS	:= $(DEFINES) -f elf
LFLAGS	:= -Bstatic -T $(TLSDIR)/ld/linker.ld -Map $(BINDIR)/aplus.map
VMFLAGS	:= -vga std -serial stdio -cdrom
MIFLAGS	:= -o

CFILES 	:= $(shell find $(SRCDIR) -type f -name "*.c")
CXXFILES:= $(shell find $(SRCDIR) -type f -name "*.cpp")
AFILES	:= $(shell find $(SRCDIR) -type f -name "*.s")
HFILES	:= $(shell find $(SRCDIR) -type f -name "*.h")
IFILES	:= $(shell find $(IRDDIR) -type f -name "*")

SFILES 	:= $(CFILES) $(CXXFILES) $(AFILES)
OFILES	:= $(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(AFILES:.s=.o)

OUTPUT	:= $(BINDIR)/aplus
RAMDISK	:= $(BINDIR)/initrd.img

DATE	:= $(shell date -R)

.PHONY: all iso run clean dist-clean

all: run

$(OUTPUT).elf : $(OFILES)
	@echo " LD\t" $(notdir $@)
	@$(LD) $(LFLAGS) -o $@ $(OFILES) $(LIBS)

.c.o:
	@echo " CC\t" $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

.cpp.o:
	@echo " CXX\t" $(notdir $<)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

.s.o:
	@echo " ASM\t" $(notdir $<)
	@$(ASM) $(AFLAGS) $< -o $@	

initrd: $(OUTPUT).elf
	@echo " MKIRD\t" $(notdir $(IFILES))
	@$(MKIRD) $(IRDDIR) $(notdir $(IFILES))
	@$(MV) $(IRDDIR)/initrd.img $(RAMDISK)

iso: initrd
	-@$(MKDIR) iso
	-@$(MKDIR) iso/boot
	-@$(MKDIR) iso/boot/grub
	@$(CP) $(OUTPUT).elf iso/aplus
	@$(CP) $(RAMDISK) iso/initrd
	@$(CP) $(TLSDIR)/grub2/grub.cfg iso/boot/grub
	@echo " MKISO\t" $(OUTPUT).iso
	@$(MKISO) $(MIFLAGS) $(OUTPUT).iso iso > /dev/null
	@$(RM) -r iso
run: iso
	@$(VM) $(VMFLAGS) $(OUTPUT).iso
clean:
	-@$(RM) $(OFILES)
	-@$(RM) -r iso

dist-clean: clean
	-@$(RM) $(OUTPUT).elf
	-@$(RM) $(OUTPUT).iso
	-@$(RM) $(RAMDISK)

git: dist-clean
	$(GIT) add .
	$(GIT) commit -a -m $(DATE)
	$(GIT) push origin master
