#ifndef _DL_H
#define _DL_H

#include <stdint.h>
#include <stddef.h>

#define DL_DEBUG



extern void* (*__dl_realloc) (void* ptr, size_t size);
extern int (*__dl_printf) (char* fmt, ...);
extern void* __dl_malloc(size_t size);
extern void __dl_free(void* ptr);

#define PUBLIC
#define PRIVATE static

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;




#define DLL_ADDRESS_SIZE	0x100000

#define PE_MACHINE_i32		0x014C
#define PE_MACHINE_i64		0x0200
#define PE_MACHINE_AMD64	0x8664


#define PE_FLAGS_NOREL		(1 << 0)
#define PE_FLAGS_EXEC		(1 << 1)
#define PE_FLAGS_NOCOFFLN	(1 << 2)
#define PE_FLAGS_NOCOFFSYM	(1 << 3)
#define PE_FLAGS_2GB_VA		(1 << 5)
#define PE_FLAGS_DWORD		(1 << 6)
#define PE_FLAGS_NODEBUG	(1 << 7)
#define PE_FLAGS_SYSTEM		(1 << 9)
#define PE_FLAGS_DLL		(1 << 10)

#define PE_MAGIC_32BIT		0x10B
#define PE_MAGIC_64BIT		0x20B
#define PE_MAGIC_ROM		0x107

#define PE_DLL_REL			(1 << 4)
#define PE_DLL_CHECK		(1 << 5)
#define PE_DLL_DEP			(1 << 6)
#define PE_DLL_SOCIAL		(1 << 7)
#define PE_DLL_SEH			(1 << 8)
#define PE_DLL_BIND			(1 << 9)
#define PE_DLL_WDM			(1 << 11)

#define PE_SECT_CODE		0x0020
#define PE_SECT_IDATA		0x0040
#define PE_SECT_UDATA		0x0080
#define PE_SECT_INFO		0x0200

#define PE_DATADIR_EXPORT	0
#define PE_DATADIR_IMPORT	1
#define PE_DATADIR_RES		2
#define PE_DATADIR_EXCEPT	3
#define PE_DATADOR_SEC		4
#define PE_DATADIR_BASEREL	5
#define PE_DATADIR_DEBUG	6
#define PE_DATADIR_DESC		7
#define PE_DATADIR_MACHINE	8
#define PE_DATADIR_TLS		9
#define PE_DATADIR_CONFIG	10
#define PE_DATADIR_COM		14

#define PE_REL_ABS			0
#define PE_REL_HIGH			1
#define PE_REL_LOW			2
#define PE_REL_HIGHLOW		3
#define PE_REL_HIGHADJ		4
#define PE_REL_MIPS_JMP		5
#define PE_REL_SECTION		6
#define PE_REL_REL32		7


#define RVA(base, addr)		\
	((uint32_t) base + (uint32_t) addr)

#define VA(base, addr)		\
	((uint32_t) base - (uint32_t) addr)


#define PE_R_TYPE(x)		\
	((x & 0xF000) >> 12)

#define PE_R_OFFSET(x)		\
	(x & 0x0FFF)

typedef struct pe_dos_header {
	uint16_t e_magic;
	uint16_t e_cblp;
	uint16_t e_cp;
	uint16_t e_crlc;
	uint16_t e_cparhdr;
	uint16_t e_minalloc;
	uint16_t e_maxalloc;
	uint16_t e_ss;
	uint16_t e_sp;
	uint16_t e_csum;
	uint16_t e_ip;
	uint16_t e_cs;
	uint16_t e_lfarlc;
	uint16_t e_ovno;
	uint16_t e_res[4];
	uint16_t e_oemid;
	uint16_t e_oeminfo;
	uint16_t e_res2[10];
	uint32_t e_lfanew;
} __attribute__((packed)) pe_dos_header_t;


typedef struct pe_file_header {
	uint16_t machine;
	uint16_t numofsect;
	uint32_t timestamp;
	uint32_t symtab;
	uint32_t symcount;
	uint16_t sizeof_opt;
	uint16_t flags;
} __attribute__((packed)) pe_file_header_t;


typedef struct pe_opt_header {
	uint16_t magic;
	uint16_t linker;
	uint32_t sizeof_code;
	uint32_t sizeof_initdata;
	uint32_t sizeof_dnitdata;
	uint32_t entry;
	uint32_t baseof_code;
	uint32_t baseof_data;
	uint32_t baseof_image;
	uint32_t section_align;
	uint32_t file_align;
	uint32_t reserved0;
	uint32_t version;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t sizeof_image;
	uint32_t sizeof_headers;
	uint32_t checksum;
	uint16_t reserved3;
	uint16_t dll_flags;
	uint32_t sizeof_stack_r;
	uint32_t sizeof_stack_c;
	uint32_t sizeof_heap_r;
	uint32_t sizeof_heap_c;
	uint32_t flags;
	uint32_t datadir_count;
	uint32_t datadir_addr;
} __attribute__((packed)) pe_opt_header_t;


typedef struct pe_section_header {
	char name[8];
	uint32_t paddr;
	uint32_t vaddr;
	uint32_t size;
	uint32_t ptr_rawdata;
	uint32_t ptr_rel;
	uint32_t ptr_ln;
	uint16_t numof_rel;
	uint16_t numof_ln;
	uint32_t flags;
} __attribute__((packed)) pe_section_header_t;


typedef struct pe_nt_header {
	uint32_t signature;
	pe_file_header_t file_header;
	pe_opt_header_t opt_header;
} __attribute__((packed)) pe_nt_header_t;


typedef struct pe_datadir {
	uint32_t rva;
	uint32_t size;
} __attribute__((packed)) pe_datadir_t;


typedef struct pe_export {
	uint32_t flags;
	uint32_t timestamp;
	uint32_t version;
	uint32_t name;
	uint32_t base;
	uint32_t numof_funcs;
	uint32_t numof_names;
	uint32_t** addrof_funcs;
	uint32_t** addrof_names;
	uint16_t** addrof_sort;
} __attribute__((packed)) pe_export_t;


typedef struct pe_base_rel {
	uint32_t vaddr;
	uint32_t size;
	uint16_t values;
} __attribute__((packed)) pe_base_rel_t;

/**
 *	\brief Runtime loaded symbol
 */
typedef struct rtsym {
	char name[255];
	void* addr;
	int flags;

	struct rtsym* next;
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
