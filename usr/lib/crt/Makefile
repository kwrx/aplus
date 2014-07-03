
TARGET := i586-elf
PREFIX := /usr/local/cross/i586-elf/lib

CC := $(TARGET)-gcc
CXX := $(TARGET)-g++
LD := $(TARGET)-ld
AR := $(TARGET)-ar
CP := cp

CFLAGS := -c -masm=intel -w

.PHONY: all clean install

all: crt0.o

crt0.o: crt0.c
	@$(CC) $(CFLAGS) $?

clean:
	-@$(RM) *.o

install:
	@$(CP) crt0.o $(PREFIX)