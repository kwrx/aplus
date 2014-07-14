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


static void* elf_lookup_symbol(Elf32_Ehdr* hdr, const char* name) {
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

	return NULL;
}

static Elf32_Shdr* elf_sheader(Elf32_Ehdr* hdr) {
	return (Elf32_Shdr*) ((int)hdr + hdr->e_shoff);
}

static Elf32_Shdr* elf_section(Elf32_Ehdr* hdr, int idx) {
	return &elf_sheader(hdr) [idx];
}

static char* elf_str_table(Elf32_Ehdr* hdr) {
	if(hdr->e_shstrndx == SHN_UNDEF)
		return NULL;

	return (char*)hdr + elf_section(hdr, hdr->e_shstrndx)->sh_offset;
}

static char* elf_lookup_string(Elf32_Ehdr* hdr, int offset) {
	char* strtab = elf_str_table(hdr);
	if(strtab == NULL)
		return NULL;

	return strtab + offset;
}

static uint32_t elf_get_symval(Elf32_Ehdr* hdr, int table, uint32_t idx) {
	if(table == SHN_UNDEF || idx == SHN_UNDEF)
		return 0;


	Elf32_Shdr* symtab = elf_section(hdr, table);
	if(idx >= symtab->sh_size) {
		kprintf("elf: Symbol index out of range (%d:%u)\n", table, idx);
		return ELF_RELOC_ERR;
	}

	uint32_t symaddr = (uint32_t) hdr + symtab->sh_offset;
	Elf32_Sym* symbol = &((Elf32_Sym*) symaddr) [idx];

	Elf32_Shdr* strtab = elf_section(hdr, symtab->sh_link);
	const char* name = (const char*) ((uint32_t) hdr + strtab->sh_offset + symbol->st_name);

	if(symbol->st_shndx == SHN_UNDEF) {
		void* addr = elf_lookup_symbol(hdr, name);
		if(addr == NULL) {
			if(ELF32_ST_BIND(symbol->st_info) & STB_WEAK) {
				return 0;
			} else {
				kprintf("elf: undefinited external symbol: %s\n", name);
				return ELF_RELOC_ERR;
			}
		} else {
			return (uint32_t) addr;
		}		
	} else if(symbol->st_shndx == SHN_ABS) {
		return symbol->st_value;
	} else {
		Elf32_Shdr* target = elf_section(hdr, symbol->st_shndx);
		return (uint32_t) hdr + symbol->st_value + target->sh_offset;
	}

	return ELF_RELOC_ERR;
}

static uint32_t elf_do_reloc(Elf32_Ehdr* hdr, Elf32_Rel* rel, Elf32_Shdr* reltab) {

	Elf32_Shdr* target = elf_section(hdr, reltab->sh_info);
	
	uint32_t addr = (uint32_t)hdr + target->sh_offset;
	uint32_t* ref = (uint32_t*) (addr + rel->r_offset);

	uint32_t symval = 0;
	if(ELF32_R_SYM(rel->r_info) != SHN_UNDEF) {
		symval = elf_get_symval(hdr, reltab->sh_link, ELF32_R_SYM(rel->r_info));
		if(symval == ELF_RELOC_ERR)
			return ELF_RELOC_ERR;
	}


	switch(ELF32_R_TYPE(rel->r_info)) {
		case R_386_NONE:
			break;

		case R_386_32:
			*ref = DO_386_32(symval, *ref);
			break;
		
		case R_386_PC32:
			*ref = DO_386_PC32(symval, *ref, (uint32_t) ref);
			break;

		default:
			kprintf("elf: unsupported relocation type (%d)\n", ELF32_R_TYPE(rel->r_info));
			return ELF_RELOC_ERR;
	}

	return symval;
}

static int elf_load_stage1(Elf32_Ehdr* hdr) {

	Elf32_Shdr* shdr = elf_sheader(hdr);

	for(uint32_t i = 0; i < hdr->e_shnum; i++) {
		Elf32_Shdr* section = &shdr[i];

		if(section->sh_type == SHT_NOBITS) {
			if(!section->sh_size)
				continue;

			if(section->sh_flags & SHF_ALLOC) {
				void* m = kmalloc(section->sh_size);
				memset(m, 0, section->sh_size);

				//section->sh_offset = (uint32_t) m - (uint32_t) hdr;
			}
		}
	}

	return 0;
}

static int elf_load_stage2(Elf32_Ehdr* hdr) {

	Elf32_Shdr* shdr = elf_sheader(hdr);
	
	uint32_t i, idx;
	for(i = 0; i < hdr->e_shnum; i++){
		Elf32_Shdr* section = &shdr[i];
		
		if(section->sh_type == SHT_REL) {
			for(idx = 0; idx < section->sh_size / section->sh_entsize; idx++) {
				Elf32_Rel* reltab = &((Elf32_Rel*)((uint32_t)hdr + section->sh_offset)) [idx];
				if(elf_do_reloc(hdr, reltab, section) == ELF_RELOC_ERR){
					kprintf("elf: failed to relocate symbol\n");
					return ELF_RELOC_ERR;
				}
			}
		}
	}

	return 0;
}

static void* elf_load_rel(Elf32_Ehdr* hdr) {

	int result = elf_load_stage1(hdr);
	if(result == ELF_RELOC_ERR) {
		kprintf("elf: unable to load relocatable ELF in stage1\n");
		return NULL;
	}

	result = elf_load_stage2(hdr);
	if(result == ELF_RELOC_ERR) {
		kprintf("elf: unable to load relocatable ELF in stage2\n");
		return NULL;
	}

	return (void*) hdr->e_entry;
}


void* elf_load_file(void* image) {
	Elf32_Ehdr *hdr = (Elf32_Ehdr*) image;
	
	if(elf_check_supported(hdr) != 0)
		return 0;
		
	switch(hdr->e_type) {
		case ET_EXEC:
			return hdr->e_entry;	
			
		case ET_REL:
		case ET_MODULE:
			return elf_load_rel(hdr);
	}

	
	return NULL;
}