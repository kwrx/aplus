SRCDIRS  += arch/x86_64/x86-family

CFLAGS   += 
ASFLAGS  += 
LDFLAGS  += -Tarch/$(PLATFORM)/link.ld -nostdlib
