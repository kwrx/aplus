#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "dl.h"


static void define_sym(dll_t* dll, char* name, void* addr, int flags) {
	rtsym_t* rt = (rtsym_t*) __dl_malloc(sizeof(rtsym_t));
	memset(rt, 0, sizeof(rtsym_t));

	strcpy(rt->name, name);
	rt->addr = addr;
	rt->flags = flags;
	rt->next = dll->symbols;
	
	dll->symbols = rt;
}

static int pe_check(pe_dos_header_t* hdr, int flags) {
#ifdef DL_DEBUG
	__dl_printf("libdl: checking valid dll\n");
#endif

	if(memcmp(&hdr->e_magic, "MZ", 2) != 0) {
		__dl_printf("libdl: No MZ Signature.\n");	
		return -1;
	}

	pe_nt_header_t* nt = (pe_nt_header_t*) RVA(hdr, hdr->e_lfanew); 
	if(memcmp(&nt->signature, "PE\0\0", 4) != 0) {
		__dl_printf("libdl: No PE Signature.\n");		
		return -1;
	}

	if(nt->file_header.machine != PE_MACHINE_i32) {
		__dl_printf("libdl: Invalid machine type %d.\n", nt->file_header.machine);
		return -1;
	}

#ifdef DL_DEBUG
	__dl_printf("libdl: success!\n");
#endif

	return 0;
}


static void pe_load(dll_t* dll) {

	pe_dos_header_t* hdr = (pe_dos_header_t*) dll->image;
	pe_nt_header_t* nt = (pe_nt_header_t*) RVA(hdr->e_lfanew, hdr);

	uint32_t size = nt->opt_header.sizeof_image;

	void* space = __dl_malloc(size);
#ifdef DL_DEBUG
	__dl_printf("libdl: loading into new space address at 0x%x (%d Bytes)\n", space, size);
#endif

	memset(space, 0, size);
	memcpy(space, dll->image, dll->imagesz);

	__dl_free(dll->image);

	dll->image = space;
	dll->imagesz = size;
}


static void pe_do_reloc(dll_t* dll, uint32_t offset) {
	pe_dos_header_t* hdr = (pe_dos_header_t*) dll->image;
	pe_nt_header_t* nt = (pe_nt_header_t*) RVA(hdr->e_lfanew, hdr);

	int delta = (int) dll->image - (int) nt->opt_header.baseof_image;

	int* v = (int*) RVA(offset, hdr);
	*v += delta;
}

static void pe_reloc(dll_t* dll) {
	pe_dos_header_t* hdr = (pe_dos_header_t*) dll->image;
	pe_nt_header_t* nt = (pe_nt_header_t*) RVA(hdr->e_lfanew, hdr);
	pe_section_header_t* shdr = (pe_section_header_t*) ((uint32_t) &nt->opt_header + nt->file_header.sizeof_opt);


#ifdef DL_DEBUG
	__dl_printf("libdl: reloc sections\n");
#endif
	int i;
	for(i = nt->file_header.numofsect - 1; i >= 0 ; i--) {
		if(!(shdr[i].flags & 0x7F))
			continue;

#ifdef DL_DEBUG
	__dl_printf("libdl: reloc section (%x) %s from 0x%x to 0x%x (%d Bytes)\n", shdr[i].flags, shdr[i].name, shdr[i].ptr_rawdata, RVA(shdr[i].vaddr, hdr), shdr[i].size);
	
	int p;
	for(p = 0; p < 128; p++)
		__dl_printf("%x ", ((char*) RVA(shdr[i].ptr_rawdata, hdr))[p] & 0xFF);
	__dl_printf("\n\n");
#endif


		memset((void*) RVA(shdr[i].vaddr, hdr), 0, shdr[i].size);

		
		if(shdr[i].size)
			memcpy((void*) RVA(shdr[i].vaddr, hdr), (void*) RVA(shdr[i].ptr_rawdata, hdr), shdr[i].size);
	}

	pe_datadir_t* datadir = (pe_datadir_t*) &nt->opt_header.datadir_addr;
	pe_base_rel_t* rel = (pe_base_rel_t*) RVA(datadir[PE_DATADIR_BASEREL].rva, hdr);


#ifdef DL_DEBUG
	__dl_printf("libdl: .reloc 0x%x (%d Bytes)\n", datadir[PE_DATADIR_BASEREL].rva, datadir[PE_DATADIR_BASEREL].size);
#endif


	if(rel->size > datadir[PE_DATADIR_BASEREL].size) {
		__dl_printf("libdl: relocation corrupted or invalid\n");
		return;
	}




	for(i = 0; i < datadir[PE_DATADIR_BASEREL].size; i++) {
		int p;
		uint16_t* v = &rel->values;

#ifdef DL_DEBUG
		__dl_printf("libdl: reloc block of offset 0x%x (%x)\n", rel->vaddr, (rel->size - 8) / sizeof(*v));
#endif

		for(p = 0; p < (rel->size - 8) / sizeof(*v); p++) {
			switch(PE_R_TYPE(v[p])) {
				case PE_REL_ABS:
					break;
				case PE_REL_HIGHLOW:
					pe_do_reloc(dll, rel->vaddr + PE_R_OFFSET(v[p]));
					break;
				default:
					__dl_printf("Unsupported relocation %d (%d)\n", p, PE_R_TYPE(v[p]));
					break;
			}
		}

		i += rel->size;
		rel = (pe_base_rel_t*) ((uint32_t) rel + rel->size);
	}
}


static void pe_load_exports(dll_t* dll) {

	pe_dos_header_t* hdr = (pe_dos_header_t*) dll->image;
	pe_nt_header_t* nt = (pe_nt_header_t*) RVA(hdr->e_lfanew, hdr);
	
	pe_datadir_t* datadir = (pe_datadir_t*) &nt->opt_header.datadir_addr;
	pe_export_t* exports = (pe_export_t*) RVA(datadir[PE_DATADIR_EXPORT].rva, hdr);


#ifdef DL_DEBUG
	__dl_printf("libdl: .edata at 0x%x (%d Bytes)\n", datadir[PE_DATADIR_EXPORT].rva, datadir[PE_DATADIR_EXPORT].size);
	__dl_printf("libdl: export directory\n\tflags: %x\n\ttimestamp: %d\n\tversion: %d\n\tname: %s\n\tnumof_funcs: %x\n\tnumof_names: %x\n", exports->flags,
																																			exports->timestamp,
																																			exports->version,
																																			RVA(exports->name, hdr),
																																			exports->numof_funcs,
																																			exports->numof_names);

	__dl_printf("\taddrof_funcs: 0x%x\n\taddrof_names: 0x%x\n\taddrof_sort: 0x%x\n", exports->addrof_funcs, exports->addrof_names, exports->addrof_sort);
#endif


	uint32_t* names = (uint32_t*) RVA(exports->addrof_names, hdr);
	uint32_t* funcs = (uint32_t*) RVA(exports->addrof_funcs, hdr);
	uint16_t* sort = (uint16_t*) RVA(exports->addrof_sort, hdr);

	int i;
	for(i = 0; i < exports->numof_funcs; i++) {
		char* name = (char*) RVA(names[i], hdr);
		void* addr = (void*) RVA(funcs[sort[i]], hdr);

		define_sym(dll, name, addr, 0);

#ifdef DL_DEBUG
		__dl_printf("libdl: exported symbol %s at 0x%x\n", name, addr);
#endif
	}
}

void* dlopen(const char* filename, int flag) {
	if(!filename) {
		errno = EINVAL;
		return NULL;
	}


	int fp = open(filename, O_RDONLY, 0644);
	if(fp < 0) {
		errno = ENOENT;
		return NULL;
	}

	dll_t* dll = (dll_t*) __dl_malloc(sizeof(dll_t));
	memset(dll, 0, sizeof(dll_t));

	strcpy(dll->name, filename);
	dll->flags = flag;
	dll->image = NULL;
	dll->imagesz = 0;
	dll->symbols = NULL;

	lseek(fp, 0, SEEK_END);
	dll->imagesz = lseek(fp, 0, SEEK_CUR);
	lseek(fp, 0, SEEK_SET);

	dll->image = (void*) __dl_malloc(dll->imagesz);
	if(read(fp, dll->image, dll->imagesz) != dll->imagesz) {
		__dl_free(dll->image);
		__dl_free(dll);

		errno = EIO;
		return NULL;
	}

	close(fp);

	if(pe_check(dll->image, 0) != 0) {
		__dl_free(dll->image);
		__dl_free(dll);

		errno = ENOEXEC;
		return NULL;
	}

	if(flag == RTLD_NOLOAD)
		return dll;
	

	pe_load(dll);
	pe_reloc(dll);
	pe_load_exports(dll);

#ifdef DL_DEBUG
	__dl_printf("libdl: library successful loaded\n");
#endif

	return dll;
}
