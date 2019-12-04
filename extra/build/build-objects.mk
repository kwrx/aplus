.PHONY: all clean distclean


all: $(TARGET)

install: $(TARGET)
	$(QUIET)echo "    INSTALL $(TARGET) -> $(DESTDIR)"
	$(QUIET)install $(TARGET) $(DESTDIR)

clean:
	$(QUIET)echo "    CLEAN   $(TARGET)"
	$(QUIET)$(RM) $(OBJS) $(DEPS)
	$(QUIET)$(RM) *.o *.d $(TARGET)

distclean: clean
	$(RM) $(DESTDIR)/$(TARGET)



-include $(DEPS)

%.d: %.c
#	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CC)  $(CFLAGS)   -MM $< -MF $@
%.d: %.S
#	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(AS)  $(ASFLAGS)  -MM $< -MF $@
%.d: %.cpp
#	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM $< -MF $@
%.d: %.cxx
#	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM $< -MF $@
%.d: %.cc
#	$(QUIET)echo "    GEN     $@"
	$(QUIET)$(CXX) $(CXXFLAGS) -MM $< -MF $@


%.o: %.c %.d
	$(QUIET)echo "    CC      $@"
	$(QUIET)$(CC)  $(CFLAGS)   -c $< -o $@
%.o: %.S %.d
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

%.o: %.s
	$(QUIET)echo "    AS      $@"
	$(AS)  $(ASFLAGS)  -c $< -o $@
%.o: %.asm
	$(QUIET)echo "    ASM     $@"
	nasm -felf64          $< -o $@

