include ../Makefile.in

O 	:= init

all: $(O)
$(O): init.c
	@echo "  CC      " $<
	@$(CC) -w -O2 -msse2 -mfpmath=sse $< -o $@ -lelfldr -lposix -lz -lm -lc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/bin

