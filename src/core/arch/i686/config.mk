SRCDIRS  += arch/x86-family

CFLAGS   += 
ASFLAGS  += 
LDFLAGS  += -Tarch/$(PLATFORM)/link.ld -nostdlib
