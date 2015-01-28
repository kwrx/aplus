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
LIBS	:= -ljvm -lc -lm -lgcc
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

ifeq ($(ARCH),rpi)
CFLAGS	+= -mcpu=arm1176jzf-s -fpic
endif


ifeq ($(ARCH),i386)
#@qemu-system-i386 -m 64 -serial stdio -cdrom aplus.iso -net nic,model=rtl8139 -net dump
VMM		:= bochs -f bochs.conf
endif
ifeq ($(ARCH),rpi)
VMM		:= qemu-system-arm -kernel aplus -initrd bin/initrd -M raspi -cpu arm1176 -serial stdio -m 256M
endif

CROSSLIB:= /opt/cross/usr
