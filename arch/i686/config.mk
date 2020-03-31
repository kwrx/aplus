SRCDIRS  += $(ROOTDIR)/arch/x86-family

CFLAGS   += 
ASFLAGS  += 
LDFLAGS  += -T$(ROOTDIR)/arch/$(PLATFORM)/link.ld -nostdlib
