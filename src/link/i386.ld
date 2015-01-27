ENTRY(_start)
SEARCH_DIR("/opt/cross/usr/lib");


SECTIONS {

	. = 0x00100000;
	
	.multiboot : AT(ADDR(.multiboot)) {
		*(.multiboot)
		*(.multiboot.*)
	}


	VBASE = 0x00000000;
	. += VBASE;
	
	.text : AT(ADDR(.text) - VBASE) {
		text = .;

		*(.text)
		*(.rodata*)
	}
	
	.data : AT(ADDR(.data) - VBASE) {
		*(.data)
	}
	
	.attribute : AT(ADDR(.attribute) - VBASE) {
		attribute_start = .;
		*(.attribute)
		attribute_end = .;
	}
	
	.bss : AT(ADDR(.bss) - VBASE) {
		bss = .;

		*(COMMON)
		*(.bss)
	}
	
	end = .;
}
