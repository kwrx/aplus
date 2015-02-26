DIRS	:= $(shell find * -maxdepth 0 -type d)

.PHONY: all clean install


all:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) all;)

clean:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) clean;)

install:
	@$(foreach dir, $(DIRS), cd $(PWD)/$(dir) && $(MAKE) install;)
