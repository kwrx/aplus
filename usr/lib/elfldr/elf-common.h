#ifndef _ELF_COMMON_H
#define _ELF_COMMON_H

#include <stdint.h>

/* File types */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

/* Supported machines */
#define EM_NONE  0		/* Reserved */
#define EM_M32   1		/* */
#define EM_SPARC 2		/* SPARC */
#define EM_386   3		/* Intel 386+ */
#define EM_68K   4		/* Motorola 68000 */
#define EM_88K   5		/* Motorola 88000 */
#define EM_486   6		/* Intel 486+ */
#define EM_860   7		/* Intel 860 */
#define EM_MIPS		8	/* MIPS R3000 (officially, big-endian only) */
#define EM_MIPS_RS4_BE 10	/* MIPS R4000 big-endian */
#define EM_PARISC      15	/* HPPA */
#define EM_SPARC32PLUS 18	/* Sun's "v8plus" */
#define EM_PPC	       20	/* PowerPC */
#define EM_PPC64       21       /* PowerPC64 */
#define EM_SH	       42	/* SuperH */
#define EM_SPARCV9     43	/* SPARC v9 64-bit */
#define EM_IA_64	50	/* HP/Intel IA-64 */
#define EM_X86_64	62	/* AMD x86-64 */
#define EM_S390		22	/* IBM S/390 */
#define EM_CRIS         76      /* Axis Communications 32-bit embedded processor */
#define EM_V850		87	/* NEC v850 */
#define EM_M32R		88	/* Renesas M32R */
#define EM_H8_300       46
#define EM_ALPHA	0x9026
#define EM_CYGNUS_V850	0x9080
#define EM_CYGNUS_M32R	0x9041
#define EM_S390_OLD     0xA390
#define EM_FRV		0x5441

/* Symbol table */

/* Binding */
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

/* Symbol type */
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

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

#endif /* _ELF_COMMON_H */
