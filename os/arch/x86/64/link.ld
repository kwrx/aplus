ENTRY(_start)

KERNEL_LOW = 0x00100000;
KERNEL_HIGH = 0xFFFFFFFF80000000;

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


	.unittest : AT(ADDR(.unittest) - KERNEL_HIGH) ALIGN(0x1000) {
		unittest_start = .;

		*(.unittest)
		*(.unittest.*)

		unittest_end = .;
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
