.PHONY: all clean
.EXPORT_ALL_VARIABLES:

ARCH		:= i386
HOST		:= i686
TARGET		:= i686-aplus
#ARCH		:= x86_64
#HOST		:= x86_64
#TARGET		:= x86_64-aplus
DEBUG		:= 1



include build/Makefile.tools
include build/Makefile.sources
include build/Makefile.flags


all:					\
	CONFIG				\
	$(KERNEL_OUTPUT)	\
	KERNEL_MODULES		\
	$(KERNEL_ISO)		\
	$(HDD)
	$(VMM)

vm:
	$(VMM)


CONFIG:
	@echo "  GEN    " config.h
	$(shell sed -i "s/#define COMMIT.*/#define COMMIT \"$(shell git rev-parse --short HEAD)\"/" config.h)
	$(shell sed -i "s/#define DEBUG.*/#define DEBUG $(DEBUG)/" config.h)
	$(shell sed -i "s/#define PLATFORM.*/#define PLATFORM \"$(ARCH)\"/" config.h)
	$(shell sed -i "s/#define TARGET.*/#define TARGET \"$(TARGET)\"/" config.h)
	$(shell sed -i "s/https:\/\/www.github.com\/kwrx\/aPlus.*/https:\/\/github.com\/kwrx\/aPlus\/commit\/$(shell git rev-parse --short HEAD)/" ./bin/etc/motd)



$(KERNEL_OUTPUT): CONFIG $(KERNEL_OBJECTS) LIBRARIES
	@echo "  LD     " $@
	@$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS) $(LIBS)
	@echo "  OBJCPY " $(KERNEL_SYM)
	@$(OBJCPY) --only-keep-debug $@ $(KERNEL_SYM)

KERNEL_MODULES: LIBRARIES
	@$(foreach dir, $(KERNEL_MODULES_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) CC=$(CC);)
	@echo "menuentry \"HDD\" {" > bin/boot/grub/grub.cfg
	@echo "multiboot /$(KERNEL_NAME) root=/dev/sda0 rootfs=ext2" >> bin/boot/grub/grub.cfg
	@$(foreach mod, $(KERNEL_MODULES), echo module /$(subst bin/,,$(mod)) >> bin/boot/grub/grub.cfg; )
	@echo "boot }" >> bin/boot/grub/grub.cfg
	@echo "menuentry \"CDROM\" {" >> bin/boot/grub/grub.cfg
	@echo "multiboot /$(KERNEL_NAME) root=/dev/cda rootfs=iso9660" >> bin/boot/grub/grub.cfg
	@$(foreach mod, $(KERNEL_MODULES), echo module /$(subst bin/,,$(mod)) >> bin/boot/grub/grub.cfg; )
	@echo "boot }" >> bin/boot/grub/grub.cfg
	
APPS: LIBRARIES
	@$(foreach dir, $(APPS_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) CC=$(CC) CXX=$(CXX) AR=$(AR);)

LIBRARIES:
	@$(foreach dir, $(LIBS_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) CC=$(CC) CXX=$(CXX) AR=$(AR);)

$(HDD): $(KERNEL_OUTPUT) KERNEL_MODULES APPS LIBRARIES
	@dd status=none if=/dev/zero of=hdd.img bs=4M count=20
	@/bin/echo -e "n\np\n\n\n\nw\n" | fdisk hdd.img >> /dev/null
	@losetup /dev/loop2 hdd.img
	@losetup /dev/loop3 hdd.img -o 1048576
	@mkfs -text2 /dev/loop3 > /dev/null
	@mkdir -p /mnt/hdd
	@mount /dev/loop3 /mnt/hdd
	@grub-install --root-directory=/mnt/hdd --force --no-floppy --modules="normal part_msdos fat multiboot biosdisk" /dev/loop2 >> /dev/null
	@cp -r bin/* /mnt/hdd
	@umount /mnt/hdd
	@losetup -D
	@qemu-img convert -f raw -O vdi hdd.img hdd.vdi
	@sync
	@rm -f hdd.img
	@sync

$(KERNEL_ISO): $(KERNEL_OUTPUT) KERNEL_MODULES APPS LIBRARIES
	grub-mkrescue -o $@ bin

.c.o:
	@echo "  CC     " $@
	@$(CC) $(CFLAGS) -o $@ $<

.s.o:
	@echo "  AS     " $@
	@$(AS) $(AFLAGS) -o $@ $<

.asm.o:
	@echo "  ASM    " $@
	@$(ASM) $(NFLAGS) -o $@ $<
	
clean_modules:
	@$(foreach dir, $(KERNEL_MODULES_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) clean;)

clean_apps:
	@$(foreach dir, $(APPS_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) clean;)

clean_libs:
	@$(foreach dir, $(LIBS_MAKE), cd $(PWD)/$(dir) && $(MAKE) -s ROOT=$(PWD) clean;)
	
clean_kernel:
	@$(RM) $(KERNEL_OBJECTS) $(KERNEL_ISO) $(KERNEL_OUTPUT) $(KERNEL_SYM)
	
clean: clean_modules clean_apps clean_kernel clean_libs
	@$(RM) -r *.o *.map

debug:
	@$(MAKE) -s DEBUG=1

release: $(KERNEL_OUTPUT)	\
		 KERNEL_MODULES		\
		 $(KERNEL_ISO)		\
		 $(HDD)
	@cd bin && tar -cf ../$(HOST)-aplus-$(KERNEL_VERSION).tar * && cd ..
	@xz -z -T2 -q $(HOST)-aplus-$(KERNEL_VERSION).tar

init:
	@mkdir -p include
	@mkdir -p build/newlib
	@mount --bind /opt/cross/include include
	@mount --bind /opt/src/newlib/3.0.0/newlib-3.0.0/newlib/libc/sys/aplus build/newlib