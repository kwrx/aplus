.PHONY: format
.NOTPARALLEL: format

DIRS := $(PROJECTS) lib libk arch include

format:
	$(QUIET)echo "    FMT     $(DIRS)"
	$(QUIET)find $(DIRS) -name "*.[ch]" -exec echo "    FMT     {}" \; -exec clang-format-18 -i {} \;


