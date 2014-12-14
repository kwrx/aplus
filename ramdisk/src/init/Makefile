include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	$(CC) -w -O0 -msse2 -mfpmath=sse *.c -o $@ -lOSMesa -lglapi -latk -lfreetype -lpng -lz -lposix -lpthread -lstdc++ -lsupc++ -lm -lc

clean:
	-$(RM) $(O)
	-$(RM) *.o

install:
	$(CP) $(O) $(PREFIX)/bin

