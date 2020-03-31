SRCDIRS  += $(ROOTDIR)/arch/x86-family

CFLAGS   += -mcmodel=kernel -mno-red-zone
ASFLAGS  += -mcmodel=kernel -mno-red-zone
LDFLAGS  += -mcmodel=kernel -mno-red-zone -z max-page-size=0x1000 -T$(ROOTDIR)/arch/$(PLATFORM)/link.ld -nostdlib
