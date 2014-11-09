#include <aplus.h>
#include <aplus/mm.h>

#include <stdint.h>
#include <errno.h>


typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;


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


#define ELF_DEBUG

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



int elf32_check(elf32_hdr_t* hdr) {

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

	return 0;
}


void* elf32_load(void* image) {
	if(image == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if(elf32_check(image) < 0)
		return NULL;

	elf32_hdr_t* hdr = (elf32_hdr_t*) image;
	elf32_shdr_t* sec = (elf32_shdr_t*) ((uint32_t) hdr->e_shoff + (uint32_t) hdr);
	
	int sn = hdr->e_shnum;
	int ss = hdr->e_shentsize;
	 
	for(int i = 0; i < sn; i++) {
		
		if(sec->sh_addr && sec->sh_offset) {

#ifdef ELF_DEBUG
			kprintf("elf: copy section to 0x%8x (%d Bytes)\n", sec->sh_addr, sec->sh_size);
#endif


			if((sec->sh_addr + sec->sh_size) < MM_UBASE || (sec->sh_addr + sec->sh_size) > (MM_UBASE + MM_USIZE))
				panic("elf section overflow");
			

			if(vmm_alloc((void*) sec->sh_addr, sec->sh_size, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER))
				memcpy((void*) sec->sh_addr, (void*) ((uint32_t) hdr + sec->sh_offset), sec->sh_size);
			else
				panic("elf: cannot allocate memory");
		}

		sec = (elf32_shdr_t*) ((uint32_t) sec + ss);
	}

#ifdef ELF_DEBUG
	kprintf("elf: entrypoint at 0x%8x\n", hdr->e_entry);
#endif

	return (void*) hdr->e_entry;
}



