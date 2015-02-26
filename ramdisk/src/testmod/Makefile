include ../Makefile.in

O 	:= testmod.k

all: $(O)
$(O):
	@$(MOD) *.c -o $@ -lposix -lc -lsys -lgcc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/mod

