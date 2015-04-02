include ../../Makefile.in

O 	:= testmod.k

all: $(O)
$(O): main.c
	@echo "  CC      " $<
	@$(MOD) $< -o $@ -lposix -lc -lsys -lgcc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/mod

