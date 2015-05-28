include ../Makefile.in

LIBS	:=	-latk			\
			-lSDL2			\
			-lSDL2_image	\
			-lSDL2_ttf		\
			-lSDL2_gfx		\
			-lfreetype		\
			-lpng			\
			-ljpeg			\
			-lposix			\
			-lpthread		\
			-lz				\
			-lc				\
			-lm				\
			-lgcc

O 	:= init

all: $(O)
$(O): init.c
	@echo "  CC      " $<
	@$(CC) -s -O2 $< -o $@ $(LIBS)

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)/bin
	-@$(RM) $(O)

