include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -w -O0 -msse2 -mfpmath=sse *.c -o $@ -lSDL2 -ldl -lposix -lz -lm -lc

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

