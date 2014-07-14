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
SRCDIR	:= $(PRJDIR)
INCDIR	:= $(PRJDIR)/../../../../src/include
LDDIR	:= /usr/local/cross/i586-elf/lib

TARGET 	:= i586-elf
PREFIX	:= $(PRJDIR)/../../bin
OUTPUT	:= $(notdir $(PRJDIR)).km

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
LIBS	:= -lx86 -lm -lc -lgcc

CFLAGS	:= $(DEFINES) -I $(INCDIR) -w -O2 -masm=intel -I include -std=c99
CXXFLAGS:= $(DEFINES) -I $(INCDIR) -w -O2 -masm=intel -I include
AFLAGS	:= $(DEFINES) -f elf
LFLAGS	:= -T $(LDDIR)/link.ld

CFILES 	:= $(shell find $(SRCDIR) -type f -name "*.c")
CXXFILES:= $(shell find $(SRCDIR) -type f -name "*.cpp")
AFILES	:= $(shell find $(SRCDIR) -type f -name "*.s")

SFILES 	:= $(CFILES) $(CXXFILES) $(AFILES)
OFILES	:= $(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(AFILES:.s=.o)


.PHONY: all clean install

all: $(OUTPUT)

$(OUTPUT): $(OFILES)
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


clean:
	@$(RM) *.o
	@$(RM) $(OUTPUT)

install:
	@$(CP) $(OUTPUT) $(PREFIX)
	
