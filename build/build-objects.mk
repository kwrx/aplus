.PHONY: all clean distclean


.DEFAULT_GOAL := all

INSTALLED = $(DESTDIR)/$(patsubst %.bin,%,$(TARGET))

all: $(TARGET)

install: $(TARGET)
	$(QUIET)echo "    INSTALL $(TARGET) -> $(INSTALLED)"
	$(QUIET)mkdir -p $(DESTDIR)
	$(QUIET)install -C $(TARGET) $(INSTALLED)

clean:
	$(QUIET)echo "    CLEAN   $(TARGET)"
	$(QUIET)$(RM) $(OBJS) $(DEPS)
	$(QUIET)$(RM) *.o *.dep $(TARGET)

distclean: clean
	$(QUIET)$(RM) $(INSTALLED)


NODEPS := clean distclean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
    -include $(DEPS)
endif


%.dep: %.c
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CC)  $(CFLAGS)   -MM -MT '$(patsubst %.c,%.o,$<)'   $< -MF $@
%.dep: %.S
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.S,%.o,$<)'   $< -MF $@
%.dep: %.s
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.s,%.o,$<)'   $< -MF $@
%.dep: %.cc
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cc,%.o,$<)'  $< -MF $@
%.dep: %.cpp
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,%.o,$<)' $< -MF $@
%.dep: %.cxx
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cxx,%.o,$<)' $< -MF $@


%.o: %.c %.dep
	$(QUIET)echo "    CC      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CC)  $(CFLAGS)   -c $< -o $@
%.o: %.S %.dep
	$(QUIET)echo "    AS      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.s %.dep
	$(QUIET)echo "    AS      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.cc %.dep
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cpp %.dep
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cxx %.dep
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.asm
	$(QUIET)echo "    ASM     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)nasm -felf64          $< -o $@
