.PHONY: format
.NOTPARALLEL: format

format:
	$(QUIET)echo "    FMT     $(PROJECTS)"
	$(QUIET)find $(PROJECTS) -name "*.[ch]" -exec echo "    FMT     {}" \; -exec clang-format-18 -i {} \;


