include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -O2 -mfpmath=sse -msse2 *.c -o $@ -ldl -latk -lposix -lm

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

