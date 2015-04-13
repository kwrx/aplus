MFLAGS	:= PREFIX=$(PREFIX) CRTMOD=$(CRTMOD) INCDIR=$(INCDIR) LINK=$(LINK) CFLAGS="$(CFLAGS)" CC=$(CC) CP=$(CP)
DIRS	:= $(shell find * -maxdepth 0 -type d)

.PHONY: all clean install


all:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) $(MFLAGS) all;)

clean:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) $(MFLAGS) clean;)

install:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) $(MFLAGS) install;)
