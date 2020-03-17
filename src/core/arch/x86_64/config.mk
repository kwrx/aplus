SRCDIRS  += arch/x86-family

CFLAGS   += -mcmodel=kernel -mno-red-zone -fno-builtin
ASFLAGS  += -mcmodel=kernel -mno-red-zone
LDFLAGS  += -mcmodel=kernel -mno-red-zone -z max-page-size=0x1000 -Tarch/$(PLATFORM)/link.ld -nostdlib
