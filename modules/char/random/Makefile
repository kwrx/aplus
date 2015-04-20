O 			:= $(notdir $(PWD)).k
CFILES		:= $(shell find . -name "*.c")

all: $(O)
$(O):
	@echo "  CC      " $@
	@$(CC) $(CFLAGS) $(CRTMOD) $(CFILES) -o $@ -T $(LINK) -I $(INCDIR) -lposix -lc -lsys -lgcc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)

