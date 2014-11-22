#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "dl.h"




static void* elf_section(elf32_hdr_t* hdr, char* name, size_t* size) {

	elf32_shdr_t* shdr = (elf32_shdr_t*) ((uint32_t) hdr + hdr->e_shoff);
	elf32_shdr_t* shstrtab = (elf32_shdr_t*) ((uint32_t) hdr + shdr[hdr->e_shstrndx].sh_offset);

	int sn = hdr->e_shnum;
	int ss = hdr->e_shentsize;
	
	int i;
	for(i = 0; i < sn; i++) {
		if(strcmp(name, (char*) ((uint32_t) shstrtab + shdr->sh_name)) == 0) {
			if(size)
				*size = shdr->sh_size;


			return (void*) ((uint32_t) hdr + shdr->sh_offset);
		}
	
		shdr = (elf32_shdr_t*) ((uint32_t) shdr + ss);
	}

	return NULL;
}


static void* elf_dynamic(elf32_hdr_t* hdr, int type) {
	size_t sz;
	elf32_dyn_t* dyn = elf_section(hdr, ".dynamic", &sz);
	if(!dyn)
		return NULL;

	int i;
	for(i = 0; i < (sz / sizeof(elf32_dyn_t)); i++) {
		if(dyn[i].d_tag == type)
			return (void*) ((uint32_t) hdr + dyn[i].d_un.d_ptr);
	}

	return NULL;
}


static int elf32_check(elf32_hdr_t* hdr, int type) {

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

static int elf_process(elf32_hdr_t* hdr, elf32_sym_t* symtab, char* strtab) {
	elf32_shdr_t* shdr = (elf32_shdr_t*) ((uint32_t) hdr + hdr->e_shoff);
	
	int i;
	for(i = 0; i < hdr->e_shnum; i++) {
		if(shdr[i].sh_type == SHT_NOBITS) {
			if(shdr[i].sh_size == 0)
				continue;

			if(shdr[i].sh_flags & SHF_ALLOC) {
				void* vm = malloc(shdr[i].sh_size);
				memset(vm, 0, shdr[i].sh_size);

				shdr[i].sh_offset = (uint32_t) vm - (uint32_t) hdr;
			}
		}
	}

	for(i = 0; i < hdr->e_shnum; i++) {
		if(shdr[i].sh_type == SHT_REL) {

			elf_rel_t* rltab = (elf32_rel_t*) ((uint32_t) hdr + shdr[i].sh_offset);			

			int idx;
			for(idx = 0; idx < (shdr[i].sh_size / shdr[i].sh_entsize); idx++) {
				 elf32_shdr_t* shtgt = shdr[shdr[i].sh_info];

				uint32_t symval = 0;
				if(ELF32_R_SYM(rltab[idx].r_info) != SHN_UNDEF) {
					elf32_sym_t* sym = &symtab[ELF32_R_SYM(rltab[idx].r_info)];

					if(sym->st_shndx == SHN_UNDEF) {
						if(!(ELF32_ST_BIND(sym->st_info) & STB_WEAK)) {
							printf("Undefined external symbol: %s\n", (char*) ((uint32_t) strtab + sym->st_name));
							return -1;
						}
					} else if(sym->st_shndx == SHN_ABS)
						symval = sym->st_value;
					else
						symval = (uint32_t) hdr + sym->st_value + shdr[sym->st_shndx].sh_offset;
				}

				int* ref = (int*) ((int)hdr + shdr[

				switch(ELF32_R_TYPE(rltab[idx].r_info)) {
					case R_386_NONE:
						break;
					case R_386_32:
						
				}
			} 
		}
	}

}


void* dlopen(const char* filename, int flag) {
	if(!filename) {
		errno = EINVAL;
		return NULL;
	}

	FILE* fp = fopen(filename, "rb");
	if(!fp) {
		errno = ENOENT;
		return NULL;
	}

	dll_t* dll = (dll_t*) malloc(sizeof(dll_t));
	memset(dll, 0, sizeof(dll_t));

	strcpy(dll->name, filename);
	dll->flags = flag;
	dll->image = NULL;
	dll->imagesz = 0;
	dll->symbols = NULL;

	fseek(fp, 0, SEEK_END);
	dll->imagesz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	dll->image = malloc(dll->imagesz);
	fread(dll->image, 1, dll->imagesz, fp);
	fclose(fp);

	if(elf32_check(dll->image, ET_DYN) != 0) {
		free(dll->image);
		free(dll);

		errno = ENOEXEC;
		return NULL;
	}

	if(flag == RTLD_NOLOAD)
		return dll;



	size_t symtabsz;
	char* strtab = elf_dynamic(dll->image, DT_STRTAB);
	elf32_sym_t* symtab = elf_section(dll->image, ".dynsym", &symtabsz);


	if(!(strtab && symtab)) {
		free(dll->image);
		free(dll);

		errno = ENOEXEC;
		return NULL;
	}


	int i;
	for(i = 0; i < (symtabsz / sizeof(elf32_sym_t)); i++) {
		printf("[%d] %s: 0x%x (%d)\n", i, (char*) (symtab[i].st_name + (uint32_t) strtab), symtab[i].st_value, symtab[i].st_info);
	}
	
	return dll;
}
