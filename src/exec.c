#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/attribute.h>
#include <aplus/elf.h>

#include <stdint.h>
#include <errno.h>


extern task_t* current_task;
extern task_t* kernel_task;


typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;


#define ET_NONE				0
#define ET_REL				1
#define ET_EXEC				2
#define ET_DYN				3
#define ET_CORE				4

#define EI_NIDENT			16
#define EI_MAG0				0
#define EI_MAG1				1
#define EI_MAG2				2
#define EI_MAG3				3
#define EI_CLASS			4
#define EI_DATA				5
#define EI_VERSION			6
#define EI_PAD				7

#define ELF_MAG0			0x7F
#define ELF_MAG1			'E'
#define ELF_MAG2			'L'
#define ELF_MAG3			'F'

#define ELF_CLASS_32		1
#define ELF_CLASS_64		2

#define ELF_DATA_LSB		1
#define ELF_DATA_MSB		2


#define SHT_NULL			0
#define SHT_PROGBITS		1
#define SHT_SYMTAB			2
#define SHT_STRTAB			3
#define SHT_RELA			4
#define SHT_HASH			5
#define SHT_DYNAMIC			6
#define SHT_NOTE			7
#define SHT_NOBITS			8
#define SHT_REL				9
#define SHT_SHLIB			10
#define SHT_DYNSYM			11

#define SHF_MASK			0xF0000000

#define SHN_UNDEF			0

#define STB_LOCAL			0
#define STB_GLOBAL			1
#define STB_WEAK			2
#define STB_LOPROC			13
#define STB_HIPROC			15

#define PT_NULL				0
#define PT_LOAD				1
#define PT_DYNAMIC			2
#define PT_INTERP			3
#define PT_NOTE				4
#define PT_SHLIB			5
#define PT_PHDR				6
#define PT_LOPROC			0x70000000
#define PT_HIPROC			0x7FFFFFFF


#define ELF32_ST_BIND(i)	((i >> 4))
#define ELF32_ST_TYPE(i)	((i) & 0x0F)

#define STT_NOTYPE			0
#define STT_OBJECT			1
#define STT_FUNC			2
#define STT_SECTION			3
#define STT_FILE			4
#define STT_LOPROC			13
#define STT_HIPROC			15

#define ELF32_R_SYM(i)		((i) >> 8)
#define ELF32_R_TYPE(i)		(i & 0xFF)
#define ELF32_R_INFO(s, t)	(((s << 8) | (t & 0xFF)))


/**
 *	\brief Enable or disable debug for ELF.
 */



/**
 *	\brief ELF32 Header.
 */
typedef struct elf32_hdr {
	uint8_t e_ident[EI_NIDENT];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
} elf32_hdr_t;

/**
 *	\brief ELF32 Section Header.
 */
typedef struct elf32_shdr {
	Elf32_Word sh_name;
	Elf32_Word sh_type;
	Elf32_Word sh_flags;
	Elf32_Addr sh_addr;
	Elf32_Off sh_offset;
	Elf32_Word sh_size;
	Elf32_Word sh_link;
	Elf32_Word sh_info;
	Elf32_Word sh_addralign;
	Elf32_Word sh_entsize;
} elf32_shdr_t;


/**
 *	\brief ELF32 Program Header.
 */
typedef struct elf32_phdr {
	Elf32_Word p_type;
	Elf32_Off p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
} elf32_phdr_t;


/**
 *	\brief ELF32 Symbol
 */
typedef struct elf32_sym_t {
	Elf32_Word st_name;
	Elf32_Addr st_value;
	Elf32_Word st_size;
	uint8_t st_info;
	uint8_t st_other;
	Elf32_Half st_shndx;
} elf32_sym_t;





static int elf_define_symbol(elf_module_t* elf, char* name, void* addr) {
	symbol_t* sym = elf->symbols;
	while(sym) {
		if(strcmp(sym->name, name) == 0) {
#ifdef ELF_DEBUG
			kprintf("elf: WARNING! symbol %s already defined at 0x%x\n", name, sym->addr);
#endif
		}

		sym = sym->next;
	}

	sym = (symbol_t*) kmalloc(sizeof(symbol_t) + strlen(name) + 1);
	strcpy(sym->name, name);
	sym->addr = addr;
	sym->next = elf->symbols;
	elf->symbols = sym;


	return 0;
}


static void* elf_resolve_symbol(elf_module_t* elf, char* name) {
	symbol_t* sym = elf->symbols;	
	while(sym) {
		if(strcmp(sym->name, name) == 0)
			return sym->addr;

		sym = sym->next;
	}


	kprintf("elf: could not resolve \"%s\"\n", name);

	return NULL;
}


static int elf_load_module(elf_module_t* elf, void* image, int size, char* start) {

	elf_module_link_cbs_t lnk = {
		.resolve = elf_resolve_symbol,
		.define = elf_define_symbol
	};


	int e;
	if((e = elf_module_init(elf, image, size)) < 0) {
#ifdef ELF_DEBUG
		kprintf("elf: elf_module_init() failed with -%d\n", -e);
#endif
		errno = ENOEXEC;
		return -1;
	}
	

	void* core = (void*) kmalloc(size);
	if(!core) {
#ifdef ELF_DEBUG
		kprintf("elf: cannot alloc memory for module\n");
#endif
		errno = ENOMEM;
		return -1;
	}


	if((e = elf_module_load(elf, core, &lnk)) < 0) {
#ifdef ELF_DEBUG
		kprintf("elf: elf_module_load() failed with -%d\n", -e);
#endif
		errno = ENOEXEC;
		return -1;
	}


	elf->start = elf_resolve_symbol(elf, start);
	kfree(image);

#ifdef ELF_DEBUG
	kprintf("elf: resolved entry %s at 0x%x\n", start, elf->start);
#endif

	return 0;
}



static int elf_load_ksyms(elf_module_t* elf) {
	list_t* syms = attribute("exports");
	if(list_empty(syms))
		return -1;

	list_foreach(v, syms) {
		struct sym_t {
			char* name;
			void* addr;
		} __packed *vs = (struct sym_t*) v;

		elf_define_symbol(elf, vs->name, vs->addr); 
	}

	list_destroy(syms);
	return 0;
}


/**
 *	\brief Check for valid ELF32 header.
 *	\param hdr ELF32 Header.
 *	\param type Type of executable.
 * 	\return 0 for valid header or -1 in case of errors.
 */
static int elf32_check(elf32_hdr_t* hdr, int type) {

	if(!hdr) {
		errno = EINVAL;	
		return -1;
	}

#ifdef ELF_DEBUG
	#define check(cond)											\
		if(cond) {												\
			kprintf("elf: condition \"%s\" failed\n", #cond);	\
			errno = ENOEXEC;									\
			return -1;											\
		}
#else
	#define check(cond)				\
		if(cond) {					\
			errno = ENOEXEC;		\
			return -1;				\
		}
#endif

	check(
		(hdr->e_ident[EI_MAG0] != ELF_MAG0) ||
		(hdr->e_ident[EI_MAG1] != ELF_MAG1) ||
		(hdr->e_ident[EI_MAG2] != ELF_MAG2) ||
		(hdr->e_ident[EI_MAG3] != ELF_MAG3)
	)

	check(hdr->e_ident[EI_CLASS] != ELF_CLASS_32)
	check(hdr->e_ident[EI_DATA] != ELF_DATA_LSB)
	check(hdr->e_type != type)

	return 0;
}


/**
 *	\brief Get Address Space of ELF32 Executable.
 *	\param hdr ELF32 Header.
 *	\param ptr Pointer to start of memory address.
 *	\param size Size of memory address.
 *	\return if success 0, otherwise -1.
 */
static int elf32_getspace(elf32_hdr_t* hdr, void** ptr, size_t* size) {
	if(elf32_check(hdr, ET_EXEC) < 0)
		return -1;

	elf32_phdr_t* phdr = (elf32_phdr_t*) ((uint32_t) hdr->e_phoff + (uint32_t) hdr);
	int pn = hdr->e_phnum;
	int ps = hdr->e_phentsize;

	int p = 0;
	int s = 0;

	for(int i = 0; i < pn; i++) {
		if(!p || p > phdr->p_vaddr)
			p = phdr->p_vaddr;

		s += (p - phdr->p_vaddr) + phdr->p_memsz;
		phdr = (elf32_phdr_t*) ((uint32_t) phdr + ps);
	}

	s &= ~0xFFFFF;
	s += 0x100000;


#ifdef ELF_DEBUG
	kprintf("elf: address space at 0x%x (%d MB)\n", p, s / 1024 / 1024);
#endif


	if(ptr)
		*ptr = (void*) p;

	if(size)
		*size = s;

	return 0;
}


/**
 *	\brief Load a ELF32 Executable image.
 *	\param image pointer to buffer address of a executable loaded in memory.
 *	\param vaddr Address of memory space needed.
 * 	\param vsize Size of memory space.
 *	\return Entry Point address.
 */
void* elf32_load(void* image, int* vaddr, int* vsize) {
	if(unlikely(image == NULL)) {
		errno = EINVAL;
		return NULL;
	}

	if(unlikely(elf32_check(image, ET_EXEC) < 0))
		return NULL;

	elf32_hdr_t* hdr = (elf32_hdr_t*) image;

	int iptr = 0, isiz = 0;
	if(unlikely(elf32_getspace(hdr, (void**) &iptr, (size_t*) &isiz) != 0))
		panic("elf: cannot found a valid address space"); 

	schedule_release(current_task);
	vmm_alloc(current_task->context.cr3, iptr, isiz, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	current_task->image->refcount++;

	if(likely(vaddr))
		*vaddr = iptr;
	if(likely(vsize))
		*vsize = isiz;


	elf32_shdr_t* sec = (elf32_shdr_t*) ((uint32_t) hdr->e_shoff + (uint32_t) hdr);
	uint32_t shstrtab = (uint32_t) hdr + sec[hdr->e_shstrndx].sh_offset;

	int sn = hdr->e_shnum;
	int ss = hdr->e_shentsize;

	for(int i = 0; i < sn; i++) {
		const char* name = (const char*) (shstrtab + sec->sh_name);

		if(likely(sec->sh_addr && sec->sh_offset)) {
#ifdef ELF_DEBUG
			kprintf("elf: copy section \"%s\" to 0x%8x [%d] (%d Bytes)\n", name, sec->sh_addr, sec->sh_type, sec->sh_size);
#endif


			if(unlikely((sec->sh_addr + sec->sh_size) < MM_UBASE || (sec->sh_addr + sec->sh_size) > (MM_UBASE + MM_USIZE)))
				panic("elf: section overflow");
			
			
			memcpy((void*) sec->sh_addr, (void*) ((uint32_t) hdr + sec->sh_offset), sec->sh_size);
			
			if(sec->sh_type == SHT_NOBITS)
				memset((void*) sec->sh_addr, 0, sec->sh_size);
		}
#ifdef ELF_DEBUG
		else
			kprintf("elf: skip section \"%s\"\n", name);
#endif

		if(strcmp(name, ".strtab") == 0)
			current_task->symbols.strtab = (uint32_t) hdr + sec->sh_offset;
		

		if(strcmp(name, ".symtab") == 0) {
			current_task->symbols.symtab = (uint32_t) hdr + sec->sh_offset;
			current_task->symbols.count = sec->sh_size;
		}



		sec = (elf32_shdr_t*) ((uint32_t) sec + ss);
	}

#ifdef ELF_DEBUG
	kprintf("elf: entrypoint at 0x%8x\n", hdr->e_entry);
#endif



	return (void*) hdr->e_entry;
}


void* elf32_load_module(void* image, int size) {
	if(unlikely(image == NULL)) {
		errno = EINVAL;
		return NULL;
	}

	if(unlikely(elf32_check(image, ET_REL) < 0)) {
		errno = ENOEXEC;
		return NULL;
	}

	elf_module_t elf;
	memset(&elf, 0, sizeof(elf_module_t));

	if(elf_load_ksyms(&elf) != 0) {
#ifdef ELF_DEBUG
		kprintf("elf: cannot load kernel symbols\n");
#endif
		return NULL;
}

	if(elf_load_module(&elf, image, size, "__init") != 0)
		return NULL;

	return (void*) elf.start;
}



char* elf_symbol_lookup(uint32_t symaddr) {
	
	
	const char* strtab = (const char*) current_task->symbols.strtab;
	elf32_sym_t* symtab = (elf32_sym_t*) current_task->symbols.symtab;
	uint32_t symtabsz = current_task->symbols.count;


	for(int i = 0; i < (symtabsz / sizeof(elf32_sym_t)); i++) {
		if(ELF32_ST_TYPE(symtab[i].st_info) != 0x02)
			continue;

		if((symaddr >= symtab[i].st_value) && (symaddr < (symtab[i].st_value + symtab[i].st_size))) {
			const char* name = (const char*) ((uint32_t) strtab + symtab[i].st_name);
			return (char*) name;
		}
	}


	strtab = (const char*) kernel_task->symbols.strtab;
	symtab = (elf32_sym_t*) kernel_task->symbols.symtab;
	symtabsz = kernel_task->symbols.count;

	for(int i = 0; i < (symtabsz / sizeof(elf32_sym_t)); i++) {
		if(ELF32_ST_TYPE(symtab[i].st_info) != 0x02)
			continue;

		if((symaddr >= symtab[i].st_value) && (symaddr < (symtab[i].st_value + symtab[i].st_size))) {
			const char* name = (const char*) ((uint32_t) strtab + symtab[i].st_name);
			return (char*) name;
		}
	}

	return NULL;
}


int exec_init_kernel_task(task_t* k) {
	elf32_shdr_t* shdr = (elf32_shdr_t*) mbd->exec.addr;
	if(unlikely(!shdr))
		return -1;

	uint32_t shstrtab = shdr[mbd->exec.shndx].sh_addr;

	for(int i = 0; i < mbd->exec.num; i++) {
		const char* name = (const char*) (shstrtab + shdr[i].sh_name);

		if(strcmp(name, ".strtab") == 0)
			k->symbols.strtab = (uint32_t) shdr[i].sh_addr;
		

		if(strcmp(name, ".symtab") == 0) {
			k->symbols.symtab = (uint32_t) shdr[i].sh_addr;
			k->symbols.count = shdr[i].sh_size;
		}
	}

	return 0;
}

EXPORT_SYMBOL(elf_symbol_lookup);

