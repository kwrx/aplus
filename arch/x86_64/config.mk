SRCDIRS  += $(ROOTDIR)/arch/x86-family

CFLAGS   += -mcmodel=kernel -mno-red-zone -march=x86-64 -mtune=generic -mfsgsbase -mrdrnd
CXXFLAGS += -mcmodel=kernel -mno-red-zone -march=x86-64 -mtune=generic -mfsgsbase -mrdrnd
ASFLAGS  += -mcmodel=kernel -mno-red-zone -march=x86-64 -mtune=generic -mfsgsbase -mrdrnd
LDFLAGS  += -mcmodel=kernel -mno-red-zone -z max-page-size=0x1000 -T$(ROOTDIR)/arch/$(PLATFORM)/link.ld -nostdlib -static -Wl,--build-id=none

RUSTFLAGS += -C no-redzone
