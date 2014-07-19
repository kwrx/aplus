STARTUP(crt0.o)
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
SEARCH_DIR("/usr/local/cross/i586-elf/lib")
SEARCH_DIR("/usr/local/cross/lib/gcc/i586-elf/4.8.2")
GROUP(-lm -lc -lgcc)

SECTIONS {
	. = 0xC0000000;
	. += SIZEOF_HEADERS;
	
	.text : ALIGN(4K) {
		*(.text)
	}
	
	.init : {
		KEEP (*(SORT_NONE(.init)))
	}
	
	.fini : {
		KEEP (*(SORT_NONE(.fini)))
	}
	
	PROVIDE (__etext = .);
	PROVIDE (_etext = .);
	PROVIDE (etext = .);
	
	
	.rodata : {
		*(.rodata.rodata.* .gnu.linkonce.r.*)
	}
	
	.rodata1 : {
		*(.rodata1)
	}
	
	.eh_frame_hdr : {
		*(.eh_frame_hdr)
	}
	
	.eh_frame : {
		*(.eh_frame)
	}
	
	.preinit_array : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}
	
	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array ))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	
	.fini_array : {
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array ))
		PROVIDE_HIDDEN (__fini_array_end = .);
	}
	
	


	.ctors : {
		PROVIDE_HIDDEN (__ctors_start = .);
		*(.ctors)
		PROVIDE_HIDDEN (__ctors_end = .);
	}
	
	.dtors : {
		PROVIDE_HIDDEN (__dtors_start = .);
		*(.dtors)
		PROVIDE_HIDDEN (__dtors_end = .);	
	}


	.data : {
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
	}


	.data1 : {
		*(.data1)
	}
	
	_edata =.;
	PROVIDE(edata = .);
	
	
	__bss_start = .;
	.bss : {
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		
		. = ALIGN(4K);
	}
	
	_end = .;
	PROVIDE(end = .);
}