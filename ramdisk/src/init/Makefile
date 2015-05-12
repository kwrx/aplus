include ../Makefile.in

O 	:= init

all: $(O)
$(O): init.c
	@echo "  CC      " $<
	@$(CC) -s -w -O2 $< -o $@ -lcairo -lpixman-1 -lfreetype -lpng -lz -lposix -lz -lm -lc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/bin

