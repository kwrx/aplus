ENTRY(_start)
SEARCH_DIR("/xdev/cross/usr/lib")
SEARCH_DIR("/xdev/cross/usr/i686-cross/lib")
SEARCH_DIR("/xdev/cross/usr/lib/gcc/arm-cross/4.9.2")

SECTIONS {
	. = 0x8000;

	.text : {
		*(.text.boot)
		*(.text)
	}

	.rodata : {
		*(.rodata)
	}

	.data : {
		*(.data)
	}

	.pool : ALIGN(0x1000) {
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



	.stack : ALIGN(0x1000) {
		*(.stack)
	}

	.bss : {
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
