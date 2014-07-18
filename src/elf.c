#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <aplus.h>
#include <aplus/elfldr.h>


#define ELF_ENTRYPOINT	"_start"

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
#define ET_MODULE	(3)

#define SHN_UNDEF	(0)
#define SHN_ABS		(0xFFF1)

#define ELF32_ST_BIND(i)	((i) >> 4)
#define ELF32_ST_TYPE(i)	((i) & 0xF)

#define ELF32_R_SYM(i)		((i) >> 8)
#define ELF32_R_TYPE(i)		((uint8_t) i)

#define ELF_RELOC_ERR -1

#define DO_386_32(S, A)			((S) + (A))
#define DO_386_PC32(S, A, P)	((S) + (A) - (P))



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

typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf32_Word st_name;
	Elf32_Addr st_value;
	Elf32_Word st_size;
	uint8_t st_info;
	uint8_t st_other;
	Elf32_Half st_shndx;
} Elf32_Sym;

typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
	Elf32_Sword r_append;
} Elf32_Rela;

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

enum ShT_Types {
	SHT_NULL = 0,
	SHT_PROGBITS = 1,
	SHT_SYMTAB = 2,
	SHT_STRTAB = 3,
	SHT_RELA = 4,
	SHT_NOBITS = 8,
	SHT_REL = 9,
};

enum ShT_Attributes {
	SHF_WRITE = 0x01,
	SHF_ALLOC = 0x02,
};

enum StT_Bindings {
	STB_LOCAL = 0,
	STB_GLOBAL = 1,
	STB_WEAK = 2,
};

enum StT_Types {
	STT_NOTYPE = 0,
	STT_OBJECT = 1,
	STT_FUNC = 2,
};

enum RtT_Types {
	R_386_NONE = 0,
	R_386_32 = 1,
	R_386_PC32 = 2,
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


static void* elf_lookup_symbol(elf_module_t* elf, const char* name) {
	elf_symbol_t* symtab = elf->e_symtab;
	while(symtab) {
		if(strcmp(symtab->name, name) == 0)
			return symtab->addr;
			
		symtab = symtab->next;
	}
	
	return NULL;
}


static void* elf_resolve_symbol(elf_module_t* elf, const char* name) {
	extern uint32_t kernel_symbols_start;
	extern uint32_t kernel_symbols_end;

	int s = (int) &kernel_symbols_start;
	int e = (int) &kernel_symbols_end;

	while(s < e) {
		struct kernel_symbol* sym = (struct kernel_symbol*) s;

		if(strcmp(sym->name, name) == 0)		
			return sym->handler;
		

		s += sizeof(struct kernel_symbol);
	}

	return elf_lookup_symbol(elf, name);
}

static int elf_define_symbol(elf_module_t* elf, char* name, void* addr) {
	elf_symbol_t* symtab = elf->e_symtab;
	
	while(symtab) {
		if(strcmp(symtab->name, name) == 0) {
			kprintf("elf: duplicate definition of \"%s\"\n", name);
			return -1;
		}
		
		symtab = symtab->next;
	}
	
	symtab = (elf_symbol_t*) malloc(sizeof(elf_symbol_t) + strlen(name) + 1);
	strcpy(symtab->name, name);
	symtab->addr = addr;
	symtab->next = elf->e_symtab;
	elf->e_symtab = symtab;
	
	return 0;
}


void* elf_load_rel(void* image, size_t size, char* symstart) {
	elf_module_t* elf = kmalloc(sizeof(elf_module_t));
	memset(elf, 0, sizeof(elf_module_t));
	
	int err;
	
	elf_module_link_cbs_t link = {
		elf_resolve_symbol,
		elf_define_symbol
	};
	
	if((err = elf_module_init(elf, image, size)) < 0) {
		kprintf("elf: could not init elf relocatable (%d)\n", err);
		return NULL;
	}
	
	void* core = kmalloc(size);
	if((err = elf_module_load(elf, core, &link)) < 0) {
		kprintf("elf: error loading elf relocatable (%d)\n", err);
		return NULL;	
	}
	
	return elf_resolve_symbol(elf, symstart);
}


void* elf_load_file(void* image, size_t size) {
	Elf32_Ehdr *hdr = (Elf32_Ehdr*) image;
	if(elf_check_supported(hdr) != 0)
		return 0;
		
	switch(hdr->e_type) {
		case ET_EXEC:
			return hdr->e_entry;	
			
		case ET_REL:
		case ET_MODULE:
			return elf_load_rel(image, size, ELF_ENTRYPOINT);
	}

	
	return NULL;
}