include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -s -w -O2 -msse2 -mfpmath=sse *.c -o $@ -latk -lSDL2 -lSDL2_ttf -lSDL2_image -lfreetype -lpng -ldl -lposix -lz -lm -lc

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

