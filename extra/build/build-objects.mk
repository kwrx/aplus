.PHONY: all clean distclean


all: $(TARGET)

install: $(TARGET)
	$(QUIET)echo "    INSTALL $(TARGET) -> $(DESTDIR)/$(patsubst %.bin,%,$(TARGET))"
	$(QUIET)mkdir -p $(DESTDIR)
	$(QUIET)install -C $(TARGET) $(DESTDIR)/$(patsubst %.bin,%,$(TARGET))

clean:
	$(QUIET)echo "    CLEAN   $(TARGET)"
	$(QUIET)$(RM) $(OBJS) $(DEPS)
	$(QUIET)$(RM) *.o *.d $(TARGET)

distclean: clean
	$(QUIET)$(RM) $(DESTDIR)/$(TARGET)



NODEPS := clean distclean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
    -include $(DEPS)
endif



%.d: %.c
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CC)  $(CFLAGS)   -MM -MT '$(patsubst %.c,%.o,$<)'   $< -MF $@
%.d: %.S
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.S,%.o,$<)'   $< -MF $@
%.d: %.s
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.s,%.o,$<)'   $< -MF $@
%.d: %.cc
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cc,%.o,$<)'  $< -MF $@
%.d: %.cpp
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,%.o,$<)' $< -MF $@
%.d: %.cxx
	$(QUIET)echo "    GEN     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cxx,%.o,$<)' $< -MF $@


%.o: %.c %.d
	$(QUIET)echo "    CC      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CC)  $(CFLAGS)   -c $< -o $@
%.o: %.S %.d
	$(QUIET)echo "    AS      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.s %.d
	$(QUIET)echo "    AS      $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.cc %.d
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cpp %.d
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cxx %.d
	$(QUIET)echo "    CXX     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.asm
	$(QUIET)echo "    ASM     $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)nasm -felf64          $< -o $@


%.a: %.toml
	$(QUIET)echo "   CARGO    $(shell realpath --relative-base=$(ROOTDIR) $@)"
	$(QUIET)cargo build --release --manifest-path $<
	$(QUIET)cp target/release/lib$(patsubst %.toml,%,$<).a $@