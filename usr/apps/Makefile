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
BINDIR 	:= $(PRJDIR)/bin
LDDIR	:= /usr/local/cross/i586-elf/lib

TARGET 	:= i586-elf
PREFIX 	:= 

CC	:= $(TARGET)-gcc
CXX	:= $(TARGET)-g++
LD	:= $(TARGET)-ld
ASM	:= nasm

CP	:= cp
MV	:= mv
MKDIR	:= mkdir
GIT	:= git


ARCH	:= X86
DEFINES	:= -D$(ARCH) -DAPLUS
LIBS	:= -lx86 -lOSMesa16 -lm -lc -lgcc

CFLAGS	:= $(DEFINES) -I $(INCDIR) -w -c -O2 -I include -std=c99
CXXFLAGS:= $(DEFINES) -I $(INCDIR) -w -c -O2 -I include
AFLAGS	:= $(DEFINES) -f elf
LFLAGS	:= -static -T $(LDDIR)/link.ld

DIRS	:= $(shell ls $(SRCDIR))


.PHONY: all clean install

all:
	@$(foreach dir, $(DIRS), cd $(SRCDIR)/$(dir) && $(MAKE) all;)

clean:
	@$(foreach dir, $(DIRS), cd $(SRCDIR)/$(dir) && $(MAKE) clean;)

install:
	@$(foreach dir, $(DIRS), cd $(SRCDIR)/$(dir) && $(MAKE) install;)
	




