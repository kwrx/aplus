#ifndef _ELF_32_H
#define _ELF_32_H

/* Base types */
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

#endif /* _ELF_32_H */
