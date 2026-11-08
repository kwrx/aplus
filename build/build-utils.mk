
vmdk: install
	$(QUIET)echo "    GEN     aplus.vmdk"
	$(QUIET)qemu-img convert -f raw -O vmdk $(TARGET) aplus.vmdk

vdi: install
	$(QUIET)echo "    GEN     aplus.vdi"
	$(QUIET)qemu-img convert -f raw -O vdi $(TARGET) aplus.vdi

vhd: install
	$(QUIET)echo "    GEN     aplus.vhd"
	$(QUIET)qemu-img convert -f raw -O vpc $(TARGET) aplus.vhd

usb: install-live
	$(QUIET)read -p "Enter the USB device path [/dev/sdb]: " USBDEV; 		\
		if [ -z "$$USBDEV" ]; then 											\
			USBDEV=/dev/sdb; 												\
		fi; 																\
		echo "    FLASH   $$USBDEV"; 										\
		sudo dd if=$(TARGET) of=$$USBDEV bs=4M status=progress conv=fsync; 	\
		sudo sync; 															\
		sudo eject $$USBDEV

setup-ssh:
	$(QUIET)echo "    SETUP   authorized_keys"
	$(QUIET)mkdir -p $(SYSROOT)/home/root/.ssh
	$(QUIET)touch $(SYSROOT)/home/root/.ssh/authorized_keys
	$(QUIET)$(shell for f in ~/.ssh/id_*.pub; do \
		if [ -f "$$f" ]; then \
			cat "$$f" >> $(SYSROOT)/home/root/.ssh/authorized_keys; \
		fi; \
	done)
	$(QUIET)chmod 600 $(SYSROOT)/home/root/.ssh/authorized_keys
	$(QUIET)chmod 700 $(SYSROOT)/home/root/.ssh
