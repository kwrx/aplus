include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -O2 *.c -o $@ -ldl -lposix

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

