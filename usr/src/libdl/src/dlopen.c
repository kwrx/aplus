#include "config.h"
#include "elf.h"
#include "dl.h"

#include <dlfcn.h>
#include <stdint.h>



static int elf_load(dll_t* dll) {
	#define R(a, b)		((void*) ((uint32_t) a + (uint32_t) b))

	Elf32_Ehdr* hdr = (Elf32_Ehdr*) dll->image;
	Elf32_Phdr* phdr = (Elf32_Phdr*) R(hdr, hdr->e_phoff);



	void* exec = (void*) dl_malloc(dll->memsz);
	dl_memset(exec, 0, dll->memsz);


	register int i;
	for(i = 0; i < hdr->e_phnum; i++) {
		if(unlikely(phdr[i].p_type != PT_LOAD))
			continue;

		if(unlikely(phdr[i].p_filesz > phdr[i].p_memsz)) {
			dl_free(exec);
	
			errno = EINVAL;
			return -1;
		}

		if(unlikely(!phdr[i].p_filesz))
			continue;

		void* s = R(hdr, phdr[i].p_offset);
		void* t = R(exec, phdr[i].p_vaddr);
		
		dl_memmove(t, s, phdr[i].p_filesz);
	}

	Elf32_Shdr* shdr = (Elf32_Shdr*) R(hdr, hdr->e_shoff);
	for(i = 0; i < hdr->e_shnum; i++) {
		if(shdr[i].sh_type == SHT_DYNSYM || shdr[i].sh_type == SHT_SYMTAB) {
			Elf32_Sym* syms = (Elf32_Sym*) R(hdr, shdr[i].sh_offset);
			const char* strings = (const char*) R(hdr, shdr[shdr[i].sh_link].sh_offset);
		
			register int j;
			for(j = 1; j < shdr[i].sh_size / sizeof(Elf32_Sym); j++) {
				if(unlikely(ELF32_ST_TYPE(syms[j].st_info) != STT_FUNC))
					continue;

				if(unlikely(ELF32_ST_BIND(syms[j].st_info) != STB_GLOBAL))
					continue;

				rtsym_t* rt = (rtsym_t*) dl_malloc(sizeof(rtsym_t));
				if(unlikely(!rt)) {
					errno = ENOMEM;
					return -1;
				}

				dl_memset(rt, 0, sizeof(rtsym_t));

				dl_strcpy(rt->name, (char*) R(strings, syms[j].st_name));
				rt->addr = R(exec, syms[j].st_value);
				rt->flags = 0;
				rt->next = dll->symbols;
	
				dll->symbols = rt;

#ifdef DEBUG
				dl_printf("libdl: exported symbol %s at 0x%x\n", (char*) rt->name, (unsigned int) rt->addr);
#endif

			}
		}
	}

	return 0;
}


void* dlopen(const char* filename, int flag) {
	if(unlikely(!filename)) {
		errno = EINVAL;
		return NULL;
	}


	int fp = dl_open(filename, O_RDONLY, 0644);
	if(unlikely(fp < 0)) {
		errno = ENOENT;
		return NULL;
	}

	dll_t* dll = (dll_t*) dl_malloc(sizeof(dll_t));
	if(unlikely(!dll)) {
		errno = ENOMEM;
		return NULL;
	}

	memset(dll, 0, sizeof(dll_t));

	dl_strcpy(dll->name, filename);
	dll->flags = flag;
	dll->image = NULL;
	dll->imagesz = 0;
	dll->symbols = NULL;

	dl_seek(fp, 0, SEEK_END);
	dll->imagesz = dl_seek(fp, 0, SEEK_CUR);
	dl_seek(fp, 0, SEEK_SET);

	dll->image = (void*) dl_malloc(dll->imagesz);
	if(unlikely(dl_read(fp, dll->image, dll->imagesz) != dll->imagesz)) {
		dl_free(dll->image);
		dl_free(dll);

		errno = EIO;
		return NULL;
	}

	close(fp);

	dll->memsz = (dll->imagesz & ~0xFFFFF) + 0x100000;

	if(unlikely(elf_check_image(dll->image) != 0)) {
		dl_free(dll->image);
		dl_free(dll);

		errno = ENOEXEC;
		return NULL;
	}


	if(flag == RTLD_NOLOAD)
		return dll;
	

	if(unlikely(elf_load(dll) != 0)) {
		dl_free(dll->image);
		dl_free(dll);

		errno = ENOEXEC;
		return NULL;
	}
	

	return dll;
}


#ifdef TEST
int main(int argc, char** argv) {
	if(argc < 2)
		return 1;

	void* h = dlopen(argv[1], 0);
	assert(h);
	return 0;
}
#endif
