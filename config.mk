SHELL	:= /bin/bash
PATH	:= /opt/cross/usr/bin:$(PATH)

TOP		:= $(PWD)

TARGET	:= i686-aplus
PREFIX	:= $(TOP)/bin


CC		:= $(TARGET)-gcc
CXX		:= $(TARGET)-g++
ASM		:= nasm
LD		:= $(TARGET)-ld
OBJCPY	:= $(TARGET)-objcopy

ZIP		:= gzip -f
CP		:= cp
MV		:= mv


DEFINES	:= -DKERNEL -DUSERNET
LIBS	:= -lpthread -lposix -lc -lm -lgcc
WARN	:= -Wno-implicit-function-declaration -Wall

CFLAGS	:= $(DEFINES) $(WARN) -I $(TOP)/src/include -c -masm=intel -nostdlib -std=c99 -g
CXXFLAGS:= $(DEFINES) $(WARN) -I $(TOP)/src/include -c -masm=intel -nostdlib
AFLAGS	:= $(DEFINES) -f elf
LFLAGS	:= -T $(TOP)/src/link.ld -Map aplus.map -L $(LIBS)


CROSSLIB:= /opt/cross/usr
