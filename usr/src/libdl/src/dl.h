#ifndef _DL_H
#define _DL_H

#include <stdint.h>

#define PUBLIC
#define PRIVATE static

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


#define DT_NULL				0
#define DT_NEEDED			1
#define DT_PLTRELSZ			2
#define DT_PLTGOT			3
#define DT_HASH				4
#define DT_STRTAB			5
#define DT_SYMTAB			6
#define DT_RELA				7
#define DT_RELASZ			8
#define DT_RELAENT			9
#define DT_STRSZ			10
#define DT_SYMENT			11
#define DT_INIT				12
#define DT_FINI				13
#define DT_SONAME			14
#define DT_RPATH			15
#define DT_SYMBOLIC			16
#define DT_REL				17
#define DT_RELSZ			18
#define DT_RELENT			19
#define DT_PLTREL			20
#define DT_DEBUG			21
#define DT_TEXTREL			22
#define DT_JMPREL			23
#define DT_LOPROC			0x70000000
#define DT_HIPROC			0x7FFFFFFF


#define R_386_NONE			0
#define R_386_32			1
#define R_386_PC32			2

/**
 *	\brief Enable or disable debug for ELF.
 */
#define ELF_DEBUG


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
 *	\brief ELF32 Symbol.
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
 *	\brief ELF32 Dynamic section
 */
typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word d_val;
		Elf32_Addr d_ptr;
	} d_un;
} elf32_dyn_t;



typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
} elf32_rel_t;

typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
	Elf32_Sword r_addend;
} elf32_rela_t;


/**
 *	\brief Runtime loaded symbol
 */
typedef struct {
	char name[255];
	void* handler;
	int flags;
} rtsym_t;

/**
 * 	\brief DLL Handle.
 */
typedef struct {
	char name[255];
	int flags;
	void* image;
	int imagesz;

	rtsym_t* symbols;
} dll_t;


#endif
