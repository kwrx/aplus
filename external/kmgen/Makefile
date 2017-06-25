.PHONY: all clean
OUTPUT		:= kmgen

all: $(OUTPUT)
$(OUTPUT): kmgen.c
	gcc $< -o $@ -include ../../include/aplus/input.h

clean:
	$(RM) $(OUTPUT)
