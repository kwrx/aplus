
TARGET := i586-elf
PREFIX := /usr/local/cross/i586-elf/lib

CC := $(TARGET)-gcc
CXX := $(TARGET)-g++
LD := $(TARGET)-ld
AR := $(TARGET)-ar
ASM := nasm
CP := cp

AFLAGS := -f elf

OUTPUT=libx86.a

.PHONY: all clean install

all: $(OUTPUT)

$(OUTPUT): int86.o
	@$(AR) -rcs $(OUTPUT) $?

int86.o: int86.s
	@$(ASM) $(AFLAGS) $?

clean:
	-@$(RM) *.o
	-@$(RM) *.a

install:
	@$(CP) $(OUTPUT) $(PREFIX)