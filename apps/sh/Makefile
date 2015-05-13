O 			:= $(notdir $(PWD))
CFILES		:= $(shell find . -name "*.c")

all: $(O)
$(O):
	@echo "  CC      " $@
	@$(CC) $(CFILES) -o $@ -lposix -lc -lsys -lgcc

clean:
	-@$(RM) $(O)

install:
	@$(CP) $(O) $(PREFIX)

