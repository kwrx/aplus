#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <aplus.h>

#define ELF_NIDENT	16

#define ELFMAG0	0x7F
#define ELFMAG1	'E' 
#define ELFMAG2	'L'  
#define ELFMAG3	'F'  
 
#define ELFDATA2LSB	(1) 
#define ELFCLASS32	(1)  

#define EM_386		(3)  
#define EV_CURRENT	(1)

#define ET_NONE 	(0)
#define ET_REL 		(1)
#define ET_EXEC		(2)

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

 
typedef struct {
	uint8_t		e_ident[ELF_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

enum Elf_Ident {
	EI_MAG0		= 0, 
	EI_MAG1		= 1, 
	EI_MAG2		= 2, 
	EI_MAG3		= 3, 
	EI_CLASS	= 4, 
	EI_DATA		= 5, 
	EI_VERSION	= 6, 
	EI_OSABI	= 7, 
	EI_ABIVERSION	= 8, 
	EI_PAD		= 9 
};
 
 
int elf_check_file(Elf32_Ehdr* hdr) {
	if(!hdr) {

		kprintf("elf: hdr not found\n");

		return 1;
	}
		
	if(hdr->e_ident[EI_MAG0] != ELFMAG0) {

		kprintf("elf: invalid magic [0]\n");

		return 1;
	}
		
	if(hdr->e_ident[EI_MAG1] != ELFMAG1) {

		kprintf("elf: invalid magic [1]\n");

		return 1;
	}	
	
	if(hdr->e_ident[EI_MAG2] != ELFMAG2) {

		kprintf("elf: invalid magic [2]\n");

		return 1;
	}
		
	if(hdr->e_ident[EI_MAG3] != ELFMAG3) {

		kprintf("elf: invalid magic [3]\n");

		return 1;
	}
		
	return 0;
}

int elf_check_supported(Elf32_Ehdr* hdr) {
	if(elf_check_file(hdr) != 0)
		return 1;
		
	if(hdr->e_ident[EI_CLASS] != ELFCLASS32) {

		kprintf("elf: unsupported class\n");

		return 1;
	}
		
	if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {

		kprintf("elf: unsupported file byte order\n");

		return 1;
	}
		
	if(hdr->e_machine != EM_386) {

		kprintf("elf: unsupported target\n");

		return 1;
	}
		
	if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {

		kprintf("elf: unsupported elf version\n");

		return 1;
	}
	
	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {

		kprintf("elf: unsupported elf type\n");

		return 1;
	}
		
	return 0;
}

void* elf_load_file(void* image) {
	Elf32_Ehdr *hdr = (Elf32_Ehdr*) image;
	
	if(elf_check_supported(hdr) != 0)
		return 0;
		
	switch(hdr->e_type) {
		case ET_EXEC:
			return hdr->e_entry;	
			
		case ET_REL:
			kprintf("elf: ET_REL not supported for now!\n");
			/* TODO */
			return NULL;
	}
	

	kprintf("elf: WTF\n");

	
	return NULL;
}