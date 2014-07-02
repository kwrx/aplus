ENTRY (_start)
SEARCH_DIR("/usr/local/cross/i586-elf/lib")
SEARCH_DIR("/usr/local/cross/lib/gcc/i586-elf/4.8.2/") 
 
SECTIONS {
    . = 0x00100000;
	
	.multiboot : AT(ADDR(.multiboot) - 0x00000000) {
		multiboot = .;
		_multiboot = .;
		__multiboot = .;
		
		*(.multiboot)
	}
 
    .text : AT(ADDR(.text) - 0x00000000) {
		text = .;
		_text = .;
		__text = .;
		
        *(.text)
    }
 
    .rodata : AT(ADDR(.rodata) - 0x00000000) {
		rodata = .;
		_rodata = .;
		__rodata = .;
		
        *(.rodata*)
    }
 
    .data : AT(ADDR(.data) - 0x00000000) {
		data = .;
		_data = .;
		__data = .;
	
        *(.data)
    }
 
    .bss : AT(ADDR(.bss) - 0x00000000) {
		bss = .;
		_bss = .;
		__bss = .;
	
        *(COMMON)
        *(.bss)
    }

	
	. =  ALIGN(0x1000);
	. += 0x1000;

	
	.syscall_handlers : AT(ADDR(.syscall_handlers) - 0x00000000) {
		syscall_handlers_start = .;
		_syscall_handlers_start = .;
		__syscall_handlers_start = .;
		
		*(.syscall_handlers)
		
		syscall_handlers_end = .;
		_syscall_handlers_end = .;
		__syscall_handlers_end = .;
	}
	
	. += 0x1000;
	. = ALIGN(0x1000);
	
	.fsys : AT(ADDR(.fsys) - 0x00000000) {
		fsys_start = .;
		_fsys_start = .;
		__fsys_start = .;
		
		*(.fsys)
		
		fsys_end = .;
		_fsys_end = .;
		__fsys_end = .;
	}
	
	. =  ALIGN(0x1000);
	. += 0x1000;
	
	end = .;
	_end = .;
	__end = .;
}