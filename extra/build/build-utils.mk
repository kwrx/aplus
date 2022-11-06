
vmdk: $(TARGET)
	$(QUIET)echo "    GEN     aplus.vmdk"
	$(QUIET)qemu-img convert -f raw -O vmdk $(TARGET) aplus.vmdk

vdi: $(TARGET)
	$(QUIET)echo "    GEN     aplus.vdi"
	$(QUIET)qemu-img convert -f raw -O vdi $(TARGET) aplus.vdi

vhd: $(TARGET)
	$(QUIET)echo "    GEN     aplus.vhd"
	$(QUIET)qemu-img convert -f raw -O vpc $(TARGET) aplus.vhd

usb: $(TARGET)
	$(QUIET)read -p "Enter the USB device path (e.g. /dev/sdb): " USBDEV; 	\
		if [ -z "$$USBDEV" ]; then 											\
			echo "No USB device path specified"; 							\
			exit 1; 														\
		fi; 																\
		echo "    GEN     $$USBDEV"; 										\
		sudo dd if=$(TARGET) of=$$USBDEV bs=4M; 							\
		sudo sync; 															\
		sudo umount $$USBDEV; 												\
		sudo eject $$USBDEV