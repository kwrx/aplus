ENTRY(_start)

KERNEL_LOW = 0x00100000;
KERNEL_HIGH = 0xC0000000;

SECTIONS {
	
	. = KERNEL_LOW;

	.multiboot : AT(ADDR(.multiboot)) {
		*(.multiboot)
		*(.multiboot.*)
	}

	. += KERNEL_HIGH;
	
	.text : AT(ADDR(.text) - KERNEL_HIGH) ALIGN(0x1000) {
		text = .;
		*(.text)
		*(.text.*)
		*(.rodata*)
	}

	.data : AT(ADDR(.data) - KERNEL_HIGH) ALIGN(0x1000) {
		data = .;
		*(.data)
		*(.data.*)
		*(COMMON)
	}


	.syscalls : AT(ADDR(.syscalls) - KERNEL_HIGH) ALIGN(0x1000) {
		syscalls_start = .;

		*(.syscalls)
		*(.syscalls.*)

		syscalls_end = .;
	}


	.exports : AT(ADDR(.exports) - KERNEL_HIGH) ALIGN(0x1000) {
		export_start = .;

		*(.exports)
		*(.exports.*)

		export_end = .;
	}

	.stack : AT(ADDR(.stack) - KERNEL_HIGH) ALIGN(0x1000) {
		*(.stack)
	}

	.bss : AT(ADDR(.bss) - KERNEL_HIGH) ALIGN(0x1000) {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		__bss_end = .;
	}

	. = ALIGN(0x1000);
	. += 0x1000;
	end = .;

	/DISCARD/ : {
		*(.comment)
		*(.eh_frame)
	}
}
