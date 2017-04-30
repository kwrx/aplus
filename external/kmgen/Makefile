.PHONY: all clean
OUTPUT		:= kmgen

all: $(OUTPUT)
$(OUTPUT): main.c
	gcc $< -o $@ -include ../../include/aplus/input.h

clean:
	$(RM) $(OUTPUT)