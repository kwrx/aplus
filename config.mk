SHELL	:= /bin/bash
PATH	:= /opt/cross/usr/bin:$(PATH)

TOP		:= $(PWD)

ARCH	:= rpi
TARGET	:= arm-none-eabi
PREFIX	:= $(TOP)/bin


CC		:= $(TARGET)-gcc
CXX		:= $(TARGET)-g++
ASM		:= nasm
AS		:= $(TARGET)-as

LD		:= $(TARGET)-ld
OBJCPY	:= $(TARGET)-objcopy

ZIP		:= gzip -f
CP		:= cp
MV		:= mv


DEFINES	:= -DKERNEL -D__aplus__ -D__$(ARCH)__ -DARCH=\"$(ARCH)\"
LIBS	:=  -lc -lm -lgcc
WARN	:= -Wno-implicit-function-declaration -Wall

CFLAGS	:= $(DEFINES) $(WARN) -I $(TOP)/src/include -c -nostdlib -std=c99 -O2
CXXFLAGS:= $(DEFINES) $(WARN) -I $(TOP)/src/include -c -nostdlib
AFLAGS	:= $(DEFINES) -f elf
ASFLAGS	:= --defsym __$(ARCH)__=0x0 -c
LFLAGS	:= -T $(TOP)/src/link/$(ARCH).ld -Map aplus.map -L $(LIBS)

ifeq ($(ARCH),i386)
CFLAGS	+= -masm=intel -mfpmath=sse -msse2
LIBS	:= -ldisasm $(LIBS)
endif


CROSSLIB:= /opt/cross/usr
