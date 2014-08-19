

#
#  Author:
#       Antonio Natale <inferdevil97@gmail.com>
#
#  Copyright (c) 2014 WareX
#
#  This program is free software: you can redistribute it and/or modify
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


include config.mk

PRJDIR 	:= $(PWD)
SRCDIR 	:= $(PRJDIR)/src
TLSDIR 	:= $(PRJDIR)/bin/tools
BINDIR 	:= $(PRJDIR)/bin
INCDIR 	:= $(PRJDIR)/src/include
USRDIR 	:= $(PRJDIR)/usr
IRDDIR	:= $(PRJDIR)/initrd
APPDIR	:= $(PRJDIR)/usr/apps/bin
MODDIR	:= $(PRJDIR)/usr/modules/bin
ULBDIR	:= /usr/local/cross/src

TARGET 	:= i586-elf
PREFIX 	:= $(BINDIR)
OUTPUT	:= $(BINDIR)/aplus
RAMDISK	:= $(BINDIR)/initrd.img
DATE	:= $(shell date -R)

CC	:= $(TARGET)-gcc
CXX	:= $(TARGET)-g++
LD	:= $(TARGET)-ld
ASM	:= nasm

CP	:= cp
MV	:= mv
MKDIR	:= mkdir
GIT	:= git

MKISO	:= grub-mkrescue
MKIRD	:= $(TLSDIR)/mkinitrd/mkinitrd
VM	:= qemu

ARCH	:= X86
LIBS	:= -lelfldr -lx86 -lm -lposix -lc -lgcc

DEFINES	:= -D$(ARCH) -DAPLUS -DAPLUS_KERNEL -DSERIAL_OUTPUT

ifeq ($(VMODE), true)
DEFINES	+= -DVIDEOMODE
endif

ifeq ($(DEBUG), true)
DEFINES	+= -DDEBUG
endif

ifeq ($(EMU), bochs)
DEFINES	+= -DBOCHS
endif


CFLAGS	:= $(DEFINES) -I $(INCDIR) -w -c -masm=intel -ffreestanding -fno-builtin -std=c99
CXXFLAGS:= $(DEFINES) -I $(INCDIR) -w -c -masm=intel -ffreestanding -fno-builtin -fno-rtti -fno-exceptions -fpermissive
AFLAGS	:= $(DEFINES) -f elf
LFLAGS	:= -Bstatic -T $(TLSDIR)/ld/linker.ld -Map $(BINDIR)/aplus.map
VMFLAGS	:= -vga std -serial stdio -cdrom $(OUTPUT).iso
MIFLAGS	:= -o

CFILES 	:= $(shell find $(SRCDIR) -type f -name "*.c")
CXXFILES:= $(shell find $(SRCDIR) -type f -name "*.cpp")
AFILES	:= $(shell find $(SRCDIR) -type f -name "*.s")
HFILES	:= $(shell find $(SRCDIR) -type f -name "*.h")
IFILES	:= $(shell find $(IRDDIR) -type f -name "*")

SFILES 	:= $(CFILES) $(CXXFILES) $(AFILES)
OFILES	:= $(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(AFILES:.s=.o)


ifeq ($(EMU), bochs)
VM		:= bochs
VMFLAGS	:= -q -f $(TLSDIR)/bochs/bochsrc.bxrc
endif





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
	


userdev: $(OUTPUT).elf
	@echo ""
	@cd $(ULBDIR) && $(MAKE) && $(MAKE) install
	@cd usr/modules && $(MAKE) && $(MAKE) install
	@cd usr/apps && $(MAKE) && $(MAKE) install

initrd: userdev
	@echo ""
	@echo " MKIRD\t" $(notdir $(IFILES))
	@$(CP) $(MODDIR)/* $(IRDDIR)
	@$(CP) $(APPDIR)/init $(IRDDIR)
	@$(MKIRD) $(IRDDIR) $(notdir $(IFILES))
	@$(MV) $(IRDDIR)/initrd.img $(RAMDISK)

iso: initrd
	-@$(MKDIR) iso
	-@$(MKDIR) iso/boot
	-@$(MKDIR) iso/boot/grub
	-@$(MKDIR) iso/bin
	-@$(MKDIR) iso/etc
	-@$(MKDIR) iso/lib
	-@$(MKDIR) iso/include
	-@$(MKDIR) iso/home
	-@$(MKDIR) iso/sys
	@$(CP) $(OUTPUT).elf iso/aplus
	@$(CP) $(RAMDISK) iso/initrd
	@$(CP) $(TLSDIR)/grub2/grub.cfg iso/boot/grub
	@$(CP) $(APPDIR)/* iso/bin
	@echo " MKISO\t" $(OUTPUT).iso
	@$(MKISO) $(MIFLAGS) $(OUTPUT).iso iso > /dev/null
	@$(RM) -r iso
run: iso
	@$(VM) $(VMFLAGS)
clean:
	-@$(RM) $(OFILES)
	-@$(RM) -r iso
	@cd $(ULBDIR) && $(MAKE) clean
	@cd usr/modules && $(MAKE) clean
	@cd usr/apps && $(MAKE) clean

dist-clean: clean
	-@$(RM) $(OUTPUT).elf
	-@$(RM) $(OUTPUT).iso
	-@$(RM) $(RAMDISK)

git: dist-clean
	@$(MKDIR) $(USRDIR)/lib
	@$(CP) $(ULBDIR)/* $(USRDIR)/lib -r
	@$(GIT) add .
	@$(GIT) commit -a -m "$(DATE)"
	@$(GIT) push origin master -f
	@$(RM) $(USRDIR)/lib -r
