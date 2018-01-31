ENTRY(_start)
SEARCH_DIR("/opt/cross/lib")
SEARCH_DIR("/opt/cross/x86_64-aplus/lib")
SEARCH_DIR("/opt/cross/lib/gcc/x86_64-aplus/7.2.0")

KERNEL_LOW = 0x00100000;
KERNEL_HIGH = 0xFFFFFFFFC0000000;

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
		*(.rodata*)
	}

	.data : AT(ADDR(.data) - KERNEL_HIGH) ALIGN(0x1000) {
		data = .;
		*(.data)
	}

	.pool : AT(ADDR(.pool) - KERNEL_HIGH) ALIGN(0x1000) {
		*(.pool)
		*(.pool.*)
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
		bss = .;
		*(.bss)
		*(COMMON)
	}

	. = ALIGN(0x1000);
	. += 0x1000;
	end = .;

	/DISCARD/ : {
		*(.comment)
		*(.eh_frame)
	}
}
