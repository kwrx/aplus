include ../Makefile.in

O 	:= init

all: $(O)
$(O):
	@$(CC) -w -O2 -msse2 -mfpmath=sse *.c -o $@ -lelfldr -lposix -lz -lm -lc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/bin

