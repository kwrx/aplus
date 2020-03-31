CFLAGS   += -mcmodel=large -mno-red-zone
ASFLAGS  += -mcmodel=large -mno-red-zone
LDFLAGS  += -mcmodel=large -mno-red-zone -z max-page-size=0x1000 -nostdlib