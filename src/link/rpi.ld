ENTRY(_start)
SEARCH_DIR("/opt/cross-arm/usr/lib");
SEARCH_DIR("/opt/cross-arm/usr/arm-none-eabi/lib/");
SEARCH_DIR("/opt/cross-arm/usr/lib/gcc/arm-none-eabi/4.8.2/");


SECTIONS {
	. = 0x8000;

	.text : {
		KEEP(*(.text.boot))
		*(.text)
	}

	. = ALIGN(4096);

	.rodata : {
		*(.rodata)
	}

	. = ALIGN(4096);
	
	.data : {
		*(.data)
	}

	. = ALIGN(4096);

	.attribute : {
		attribute_start = .;
		*(.attribute)
		attribute_end = .;
	}

	. = ALIGN(4096);

	__bss_start__ = .;
	.bss : {
		bss = .;
		*(COMMON)
		*(.bss)
	}
	
	. = ALIGN(4096);
	__bss_end__ = .;
	__end__ = .;

	end = .;
}
