include ../Makefile.in

O 	:= testmod.k

all: $(O)
$(O):
	$(CC) -nostartfiles -r -s -O0 *.c -lposix -lc -lsys -o $@.o
	$(MOD) $@.o -o $@
clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/mod

