
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
	$(QUIET)read -p "Enter the USB device path (e.g. /dev/sdb): " USBDEV; 	\
		if [ -z "$$USBDEV" ]; then 											\
			echo "No USB device path specified"; 							\
			exit 1; 														\
		fi; 																\
		echo "    FLASH   $$USBDEV"; 										\
		sudo dd if=$(TARGET) of=$$USBDEV bs=4M; 							\
		sudo sync; 															\
		sudo eject $$USBDEV