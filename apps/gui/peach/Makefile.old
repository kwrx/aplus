.PHONY: all clean

OUTPUT      ?=
CC          ?=
CXX         ?=
ROOT        ?=
OUTPUT      := $(ROOT)/root/usr/sbin/$(notdir $(PWD))

CFILES      :=  $(shell $(FIND) $(PWD)/* -type f -name "*.c")
CPPFILES    :=  $(shell $(FIND) $(PWD)/* -type f -name "*.cpp")
OFILES      :=  $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

CFLAGS      :=  -pipe -I include -std=gnu99 -O3 -c
CPPFLAGS    :=  -pipe -I include -O3 -c

all: $(OUTPUT)
$(OUTPUT): $(OFILES)
	@echo "  CCLD   " $(subst $(ROOT)/,,$@)
	@$(CC) -pipe -O3 -s -o $@ $(OFILES) -lcairo -lpixman-1 -lfreetype -lwebp -lpng -lz -laplus -lpthread -lstdc++ -lsupc++ -lc -lgcc


.c.o:
	@echo "  CC     " $(subst $(ROOT)/,,$@)
	@$(CC) $(CFLAGS) -o $@ $<

.cpp.o:
	@echo "  CXX    " $(subst $(ROOT)/,,$@)
	@$(CXX) $(CPPFLAGS) -o $@ $<

	
clean:
	$(RM) $(OFILES) $(OUTPUT)