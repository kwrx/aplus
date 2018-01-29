#ifndef _DL_H
#define _DL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <aplus/base.h>

#if defined (__i386__) || defined(__arm__)
typedef uint8_t Elf_Byte;
typedef uint32_t Elf_Addr;
typedef uint16_t Elf_Half;
typedef uint32_t Elf_Off;
typedef int32_t Elf_Sword;
typedef uint32_t Elf_Word;

#define ELF_SYM_BIND(x)    ((x) >> 4)
#define ELF_SYM_TYPE(x)    (((unsigned int) x) & 0xf)

#define ELF_REL_SYM(x)    ((x) >> 8)
#define ELF_REL_TYPE(x)    ((x) & 0xff)

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

#elif defined(__x86_64__) || defined(__aarch64__)

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

#define ELF_SYM_BIND(x)    ((x) >> 4)
#define ELF_SYM_TYPE(x)    (((unsigned int) x) & 0xf)

#define ELF_REL_SYM(i)    ((i) >> 32)
#define ELF_REL_TYPE(i)    ((i) & 0xffffffff)

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


#endif



/* File types */
#define ET_NONE     0
#define ET_REL    1
#define ET_EXEC     2
#define ET_DYN    3
#define ET_CORE     4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

/* Supported machines */
#define EM_NONE    0        /* Reserved */
#define EM_M32     1        /* */
#define EM_SPARC 2        /* SPARC */
#define EM_386     3        /* Intel 386+ */
#define EM_68K     4        /* Motorola 68000 */
#define EM_88K     5        /* Motorola 88000 */
#define EM_486     6        /* Intel 486+ */
#define EM_860     7        /* Intel 860 */
#define EM_MIPS        8    /* MIPS R3000 (officially, big-endian only) */
#define EM_MIPS_RS4_BE 10    /* MIPS R4000 big-endian */
#define EM_PARISC        15    /* HPPA */
#define EM_SPARC32PLUS 18    /* Sun's "v8plus" */
#define EM_PPC             20    /* PowerPC */
#define EM_PPC64         21         /* PowerPC64 */
#define EM_SH             42    /* SuperH */
#define EM_SPARCV9     43    /* SPARC v9 64-bit */
#define EM_IA_64    50    /* HP/Intel IA-64 */
#define EM_X86_64    62    /* AMD x86-64 */
#define EM_S390        22    /* IBM S/390 */
#define EM_CRIS         76        /* Axis Communications 32-bit embedded processor */
#define EM_V850        87    /* NEC v850 */
#define EM_M32R        88    /* Renesas M32R */
#define EM_H8_300         46
#define EM_ALPHA    0x9026
#define EM_CYGNUS_V850    0x9080
#define EM_CYGNUS_M32R    0x9041
#define EM_S390_OLD     0xA390
#define EM_FRV        0x5441

/* Symbol table */

/* Binding */
#define STB_LOCAL    0
#define STB_GLOBAL 1
#define STB_WEAK     2

/* Symbol type */
#define STT_NOTYPE    0
#define STT_OBJECT    1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

/* sh_type */
#define SHT_NULL    0
#define SHT_PROGBITS    1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA    4
#define SHT_HASH    5
#define SHT_DYNAMIC    6
#define SHT_NOTE    7
#define SHT_NOBITS    8
#define SHT_REL        9
#define SHT_SHLIB    10
#define SHT_DYNSYM    11
#define SHT_NUM        12
#define SHT_LOPROC    0x70000000
#define SHT_HIPROC    0x7fffffff
#define SHT_LOUSER    0x80000000
#define SHT_HIUSER    0xffffffff

/* sh_flags */
#define SHF_WRITE    0x1
#define SHF_ALLOC    0x2
#define SHF_EXECINSTR    0x4
#define SHF_MASKPROC    0xf0000000

/* special section indexes */
#define SHN_UNDEF    0
#define SHN_LORESERVE    0xff00
#define SHN_LOPROC    0xff00
#define SHN_HIPROC    0xff1f
#define SHN_ABS        0xfff1
#define SHN_COMMON    0xfff2
#define SHN_HIRESERVE    0xffff

#define ELF_MAGIC    "\177ELF"

#define R_386_32    1
#define R_386_PC32    2



struct elf_ehdr;
struct elf_shdr;


/*
 * Error codes.
 */
enum {
    EME_OK = 0,
    EME_NOENT = 1,
    EME_NOEXEC = 2,
    EME_UNSUPPORTED = 3,
    EME_UNDEFINED_REFERENCE = 4,
    EME_MULTIPLE_DEFINITIONS = 5,
    EME_NOMEM = 6,
    EME_IO = 7,
};


typedef struct symbol {
    struct symbol* next;

    void* addr;
    char name[1];    
} symbol_t;

typedef struct dl {
    char* libname;
    int flags;

    struct elf_ehdr *header;
    struct elf_shdr *sections;
    struct elf_phdr *programs;
    struct elf_shdr *symtab;
    struct elf_shdr *strtab;

    char *names;
    char *strings;

    size_t size;
    void *start;

    void *data; /* user data */
    symbol_t* symbols;

    int refcount;
    struct dl* next;
} dl_t;


extern dl_t* dl_libs;
extern char* dl_path[];
extern int __dlerrno;


extern int __dl_define(dl_t* dl, char* name, void* value);
extern void* __dl_resolve(dl_t* dl, char* name);


#define DEBUG 1

#endif
