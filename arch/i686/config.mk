SRCDIRS  += $(ROOTDIR)/arch/x86-family

CFLAGS   += -march=i686 -mtune=generic -mrdrnd
ASFLAGS  += -march=i686 -mtune=generic -mrdrnd
LDFLAGS  += -T$(ROOTDIR)/arch/$(PLATFORM)/link.ld -nostdlib
