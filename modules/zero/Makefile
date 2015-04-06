O 	:= $(notdir $(PWD)).k
CC	:= i686-aplus-gcc
CP	:= cp

all: $(O)
$(O):
	@echo "  CC      " $@
	@$(CC) -Wno-implicit-function-declaration -std=c99 -nostdlib -fno-common -r -s ../crtmod.c *.c -o $@ -T../link.ld -I ../../src/include -lposix -lc -lsys -lgcc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) ../../ramdisk/mod

