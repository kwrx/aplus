include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -s -w -O2 -msse2 -mfpmath=sse *.c -o $@ -ldl -lcairo -lpixman-1 -latk -lpng -lz -lposix -lm

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

