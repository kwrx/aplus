ENTRY(_start)

KERNEL_LMA = 1M;
KERNEL_VMA = 0xC0000000;

SECTIONS {

	.  = KERNEL_LMA;
	. += KERNEL_VMA;
	
	.text BLOCK(4K) : AT(ADDR(.text) - KERNEL_VMA) ALIGN(4K) {
		text = .;
		
		*(.multiboot)
		*(.multiboot.*)
		*(.text)
		*(.text.*)
		*(.rodata*)
	}


	.data BLOCK(4K) : AT(ADDR(.data) - KERNEL_VMA) ALIGN(4K) {
		data = .;
		
		*(.data)
		*(.data.*)
		*(COMMON)
	}


	.syscalls BLOCK(4K) : AT(ADDR(.syscalls) - KERNEL_VMA) ALIGN(4K) {
		syscalls_start = .;

		*(.syscalls)
		*(.syscalls.*)

		syscalls_end = .;
	}


	.bss BLOCK(4K) : AT(ADDR(.bss) - KERNEL_VMA) ALIGN(4K) {
		__bss_start = .;

		*(.bss)
		*(.bss.*)
		
		__bss_end = .;
	}



	. = ALIGN(4K);
	. += 0x1000;
	end = .;


	/DISCARD/ : {
		*(.comment)
		*(.eh_frame)
	}
	
}
