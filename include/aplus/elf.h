#ifndef _APLUS_ELF_H
#define _APLUS_ELF_H

#include <stdint.h>
#include <sys/types.h>

typedef struct symbol {
	void* address;
	char name[1];	
} symbol_t;

#if defined(__i386__)
typedef uint8_t Elf_Byte;
typedef uint32_t Elf_Addr;
typedef uint16_t Elf_Half;
typedef uint32_t Elf_Off;
typedef int32_t Elf_Sword;
typedef uint32_t Elf_Word;

#define ELF_SYM_BIND(x)	((x) >> 4)
#define ELF_SYM_TYPE(x)	(((unsigned int) x) & 0xf)

#define ELF_REL_SYM(x)	((x) >> 8)
#define ELF_REL_TYPE(x)	((x) & 0xff)

/* Relocation */
typedef struct {
	Elf_Addr r_offset;
	Elf_Word r_info;
} Elf_Rel;

typedef struct {
	Elf_Addr r_offset;
	Elf_Word r_info;
	Elf_Sword r_addend;
} Elf_Rela;

typedef struct {
	Elf_Sword d_tag;
	union {
		Elf_Word d_val;
		Elf_Addr d_ptr;
	} d_un;
} Elf_Dyn;

/* Symbol */
typedef struct {
	Elf_Word st_name;
	Elf_Addr st_value;
	Elf_Word st_size;
	Elf_Byte st_info;
	Elf_Byte st_other;
	Elf_Half st_shndx;
} Elf_Sym;

/* ELF header */
typedef struct elf_ehdr {
	Elf_Byte e_ident[16];
	Elf_Half e_type;
	Elf_Half e_machine;
	Elf_Word e_version;
	Elf_Addr e_entry;
	Elf_Off e_phoff;
	Elf_Off e_shoff;
	Elf_Word e_flags;
	Elf_Half e_ehsize;
	Elf_Half e_phentsize;
	Elf_Half e_phnum;
	Elf_Half e_shentsize;
	Elf_Half e_shnum;
	Elf_Half e_shstrndx;
} Elf_Ehdr;

/* Section header */
typedef struct elf_shdr {
	Elf_Word sh_name;
	Elf_Word sh_type;
	Elf_Word sh_flags;
	Elf_Addr sh_addr;
	Elf_Off sh_offset;
	Elf_Word sh_size;
	Elf_Word sh_link;
	Elf_Word sh_info;
	Elf_Word sh_addralign;
	Elf_Word sh_entsize;
} Elf_Shdr;


typedef struct elf_phdr {
	Elf_Word p_type;
	Elf_Off p_offset;
	Elf_Addr p_vaddr;
	Elf_Addr p_paddr;
	Elf_Word p_filesz;
	Elf_Word p_memsz;
	Elf_Word p_flags;
	Elf_Word p_align;
} Elf_Phdr;

typedef Elf_Sym elf32_sym_t;
typedef Elf_Shdr elf32_shdr_t;
typedef Elf_Phdr elf32_phdr_t;
typedef Elf_Ehdr elf32_hdr_t;

#define ELF_ARCH	EM_386



#elif defined(__x86_64__)

/* 64-bit ELF base types. */
typedef uint8_t Elf_Byte;
typedef uint64_t Elf_Addr;
typedef uint16_t Elf_Half;
typedef int16_t Elf_SHalf;
typedef uint64_t Elf_Off;
typedef int32_t Elf_Sword;
typedef uint32_t Elf_Word;
typedef uint64_t Elf_Xword;
typedef int64_t Elf_Sxword;

#define ELF_SYM_BIND(x)	((x) >> 4)
#define ELF_SYM_TYPE(x)	(((unsigned int) x) & 0xf)

#define ELF_REL_SYM(i)	((i) >> 32)
#define ELF_REL_TYPE(i)	((i) & 0xffffffff)

/* Relocation */
typedef struct {
	Elf_Addr r_offset;
	Elf_Xword r_info;
} Elf_Rel;

typedef struct {
	Elf_Addr r_offset;
	Elf_Xword r_info;
	Elf_Sxword r_addend;
} Elf_Rela;

typedef struct {
	Elf_Sword d_tag;
	union {
		Elf_Word d_val;
		Elf_Addr d_ptr;
	} d_un;
} Elf_Dyn;

/* Symbol */
typedef struct {
	Elf_Word st_name;
	Elf_Byte st_info;
	Elf_Byte st_other;
	Elf_Half st_shndx;
	Elf_Addr st_value;
	Elf_Xword st_size;
} Elf_Sym;

/* ELF header */
typedef struct {
	Elf_Byte e_ident[16];
	Elf_Half e_type;
	Elf_Half e_machine;
	Elf_Word e_version;
	Elf_Addr e_entry;
	Elf_Off e_phoff;
	Elf_Off e_shoff;
	Elf_Word e_flags;
	Elf_Half e_ehsize;
	Elf_Half e_phentsize;
	Elf_Half e_phnum;
	Elf_Half e_shentsize;
	Elf_Half e_shnum;
	Elf_Half e_shstrndx;
} Elf_Ehdr;

/* Section header */
typedef struct {
	Elf_Word sh_name;
	Elf_Word sh_type;
	Elf_Xword sh_flags;
	Elf_Addr sh_addr;
	Elf_Off sh_offset;
	Elf_Xword sh_size;
	Elf_Word sh_link;
	Elf_Word sh_info;
	Elf_Xword sh_addralign;
	Elf_Xword sh_entsize;
} Elf_Shdr;

typedef struct elf_phdr {
	Elf_Word p_type;
	Elf_Word p_flags;
	Elf_Off p_offset;
	Elf_Addr p_vaddr;
	Elf_Addr p_paddr;
	Elf_Xword p_filesz;
	Elf_Xword p_memsz;
	Elf_Xword p_align;
} Elf_Phdr;

#define ELF_ARCH	EM_X86_64
#else
#error "elf.h: unknown platform!"
#endif




/* File types */
#define ET_NONE	 0
#define ET_REL	1
#define ET_EXEC	 2
#define ET_DYN	3
#define ET_CORE	 4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

/* Supported machines */
#define EM_NONE	0		/* Reserved */
#define EM_M32	 1		/* */
#define EM_SPARC 2		/* SPARC */
#define EM_386	 3		/* Intel 386+ */
#define EM_68K	 4		/* Motorola 68000 */
#define EM_88K	 5		/* Motorola 88000 */
#define EM_486	 6		/* Intel 486+ */
#define EM_860	 7		/* Intel 860 */
#define EM_MIPS		8	/* MIPS R3000 (officially, big-endian only) */
#define EM_MIPS_RS4_BE 10	/* MIPS R4000 big-endian */
#define EM_PARISC		15	/* HPPA */
#define EM_SPARC32PLUS 18	/* Sun's "v8plus" */
#define EM_PPC			 20	/* PowerPC */
#define EM_PPC64		 21		 /* PowerPC64 */
#define EM_SH			 42	/* SuperH */
#define EM_SPARCV9	 43	/* SPARC v9 64-bit */
#define EM_IA_64	50	/* HP/Intel IA-64 */
#define EM_X86_64	62	/* AMD x86-64 */
#define EM_S390		22	/* IBM S/390 */
#define EM_CRIS		 76		/* Axis Communications 32-bit embedded processor */
#define EM_V850		87	/* NEC v850 */
#define EM_M32R		88	/* Renesas M32R */
#define EM_H8_300		 46
#define EM_ALPHA	0x9026
#define EM_CYGNUS_V850	0x9080
#define EM_CYGNUS_M32R	0x9041
#define EM_S390_OLD	 0xA390
#define EM_FRV		0x5441

/* Program Header */
#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7FFFFFFF

/* Binding */
#define STB_LOCAL	0
#define STB_GLOBAL 	1
#define STB_WEAK	2
#define STT_NUM		3

/* Symbol type */
#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION 3
#define STT_FILE	4
#define STT_COMMON	5
#define STT_TLS		6
#define STT_NUM		7
#define STT_LOPROC	13
#define STT_HIPROC	15

/* sh_type */
#define SHT_NULL	0
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11
#define SHT_NUM		12
#define SHT_LOPROC	0x70000000
#define SHT_HIPROC	0x7fffffff
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xffffffff

/* sh_flags */
#define SHF_WRITE	0x1
#define SHF_ALLOC	0x2
#define SHF_EXECINSTR	0x4
#define SHF_MASKPROC	0xf0000000

/* special section indexes */
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_HIRESERVE	0xffff

#define ELF_MAGIC	"\177ELF"

#define R_386_32	1
#define R_386_PC32	2



struct elf_ehdr;
struct elf_shdr;







#endif