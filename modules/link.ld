ENTRY(_start)
SEARCH_DIR("/opt/cross/usr/lib");

SECTIONS {

	. = 0;

	.text : {
		*(.text)
		*(.rodata*)
	}


	.data : {
		*(.data)
	}

	.attribute : {
		attribute_start = .;
		*(.attribute)
		attribute_end = .;
	}
	
	.bss : {
		bss = .;

		*(COMMON)
		*(.bss)
	}
	
	end = .;
	_end = .;
}
