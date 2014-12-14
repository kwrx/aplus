#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>

#include <stdint.h>
#include <errno.h>

#include <grub.h>


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
#define ET_LOPROC			0xFF00
#define ET_HIPROC			0xFFFF

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

#define SHF_WRITE			1
#define SHF_ALLOC			2
#define SHF_EXECINSTR		4
#define SHF_MASK			0xF0000000

#define SHN_UNDEF			0
#define SHN_LORESERVE		0xFF00
#define SHN_LOPROC			0xFF00
#define SHN_HIPROC			0xFF1F
#define SHN_ABS				0xFFF1
#define SHN_COMMON			0xFFF2
#define SHN_HIRESERVE		0xFFFF

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

/**
 *	\brief Check for valid ELF32 header.
 *	\param hdr ELF32 Header.
 *	\param type Type of executable.
 * 	\return 0 for valid header or -1 in case of errors.
 */
int elf32_check(elf32_hdr_t* hdr, int type) {

	if(!hdr) {
		errno = EINVAL;	
		return -1;
	}

	#define check(cond)				\
		if(cond) {					\
			errno = ENOEXEC;		\
			return -1;				\
		}

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
int elf32_getspace(elf32_hdr_t* hdr, void** ptr, size_t* size) {
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

		s += phdr->p_memsz;
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
	if(image == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(elf32_check(image, ET_EXEC) < 0)
		return NULL;

	elf32_hdr_t* hdr = (elf32_hdr_t*) image;

	int iptr, isiz;
	if(elf32_getspace(hdr, (void**) &iptr, (size_t*) &isiz) != 0)
		panic("elf: cannot found a valid address space"); 

	schedule_release(current_task);
	vmm_alloc(current_task->context.cr3, iptr, isiz, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);


	if(vaddr)
		*vaddr = iptr;
	if(vsize)
		*vsize = isiz;


	elf32_shdr_t* sec = (elf32_shdr_t*) ((uint32_t) hdr->e_shoff + (uint32_t) hdr);
	uint32_t shstrtab = (uint32_t) hdr + sec[hdr->e_shstrndx].sh_offset;

	int sn = hdr->e_shnum;
	int ss = hdr->e_shentsize;

	for(int i = 0; i < sn; i++) {
		const char* name = (const char*) (shstrtab + sec->sh_name);

		if(sec->sh_addr && sec->sh_offset) {
#ifdef ELF_DEBUG
			kprintf("elf: copy section \"%s\" to 0x%8x [%d] (%d Bytes)\n", name, sec->sh_addr, sec->sh_type, sec->sh_size);
#endif


			if((sec->sh_addr + sec->sh_size) < MM_UBASE || (sec->sh_addr + sec->sh_size) > (MM_UBASE + MM_USIZE))
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
	elf32_shdr_t* shdr = (elf32_shdr_t*) mbd->addr;
	uint32_t shstrtab = shdr[mbd->shndx].sh_addr;

	for(int i = 0; i < mbd->num; i++) {
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



