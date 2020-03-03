.PHONY: all clean distclean


all: $(TARGET)

install: $(TARGET)
	$(QUIET)echo "    INSTALL $(TARGET) -> $(DESTDIR)/$(patsubst %.bin,%,$(TARGET))"
	$(QUIET)mkdir -p $(DESTDIR)
	$(QUIET)install $(TARGET) $(DESTDIR)/$(patsubst %.bin,%,$(TARGET))

clean:
	$(QUIET)echo "    CLEAN   $(TARGET)"
	$(QUIET)$(RM) $(OBJS) $(DEPS)
	$(QUIET)$(RM) *.o *.d $(TARGET)

distclean: clean
	$(RM) $(DESTDIR)/$(TARGET)



NODEPS := clean distclean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
    -include $(DEPS)
endif



%.d: %.c
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CC)  $(CFLAGS)   -MM -MT '$(patsubst %.c,%.o,$<)'   $< -MF $@
%.d: %.S
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.S,%.o,$<)'   $< -MF $@
%.d: %.s
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM -MT '$(patsubst %.s,%.o,$<)'   $< -MF $@
%.d: %.cpp
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,%.o,$<)' $< -MF $@
%.d: %.cxx
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cxx,%.o,$<)' $< -MF $@
%.d: %.cc
	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cc,%.o,$<)'  $< -MF $@



%.o: %.c %.d
	$(QUIET)echo "    CC      $@"
	$(QUIET)$(CC)  $(CFLAGS)   -c $< -o $@
%.o: %.S %.d
	$(QUIET)echo "    AS      $@"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.s %.d
	$(QUIET)echo "    AS      $@"
	$(QUIET)$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.cpp %.d
	$(QUIET)echo "    CXX     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cxx %.d
	$(QUIET)echo "    CXX     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cc %.d
	$(QUIET)echo "    CXX     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.asm
	$(QUIET)echo "    ASM     $@"
	$(QUIET)nasm -felf64          $< -o $@
