/*
 * ELF loadable modules
 *
 * 12 Feb 2011, Yury Ossadchy
 */
#include <string.h>
#include <aplus/elf.h>

static char *elf_module_sym_name(elf_module_t *elf, int offs)
{
    return elf->names + offs;
}

static void elf_module_layout(elf_module_t *elf)
{
    int i;
    Elf_Shdr *shdr;

    for (i = 1; i < elf->header->e_shnum; i++) {
	shdr = &elf->sections[i];

	if (!(shdr->sh_flags & SHF_ALLOC))
	    continue;

	shdr->sh_addr = shdr->sh_addralign
	    ? (elf->size + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1)
	    : elf->size;
	elf->size = shdr->sh_addr + shdr->sh_size;
    }
}

static void *elf_module_get_ptr(elf_module_t *elf, Elf_Addr addr)
{
    return elf->start + addr;
}

static void *elf_module_sec_ptr(elf_module_t *elf, Elf_Shdr *shdr)
{
    return (void *) elf->header + shdr->sh_offset;
}

static int elf_module_reloc(elf_module_t *elf)
{
    int i;
    Elf_Shdr *shdr = &elf->sections[0];

    for (i = 0; i < elf->header->e_shnum; i++) {
	switch (shdr->sh_type) {
	case SHT_REL:
	    elf_module_reloc_section(elf, shdr);
	    break;
	case SHT_RELA:
	    elf_module_reloca_section(elf, shdr);
	    break;
	}
	shdr++;
    }
    return 0;
}

static int elf_module_link(elf_module_t *elf, elf_module_link_cbs_t *cbs)
{
    int err;
    int n = elf->symtab->sh_size / sizeof(Elf_Sym);
    Elf_Sym *sym;
    Elf_Sym *symtab = elf_module_sec_ptr(elf, elf->symtab);
    Elf_Sym *end = &symtab[n];

    for (sym = &symtab[1]; sym < end; sym++) {
	switch (sym->st_shndx) {
	case SHN_COMMON:
	    return -EME_NOEXEC;

	case SHN_ABS:
	    break;
	
	case SHN_UNDEF:
	    /* resolve external symbol */
	    sym->st_value = (Elf_Addr)
		cbs->resolve(elf, elf_module_sym_name(elf, sym->st_name));
	    if (!sym->st_value)
		return -EME_UNDEFINED_REFERENCE;
	    break;

	default:
	    /* bind to physical section location and define as accessible symbol */
	    sym->st_value += (Elf_Addr) elf_module_get_ptr(elf,
		elf->sections[sym->st_shndx].sh_addr);

	    if (ELF_SYM_TYPE(sym->st_info) != STT_SECTION) {
		err = cbs->define(elf, elf_module_sym_name(elf, sym->st_name),
		    (void *) sym->st_value);
		if (err < 0)
		    return err;
	    }
	}
    }
    return 0;
}

int elf_module_init(elf_module_t *elf, void *data, size_t size)
{
    int i;

    elf->header = data;

    if (memcmp(elf->header->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1)
	|| !elf_module_check_machine(elf)) {
	return -EME_NOEXEC;
    }

    elf->sections = data + elf->header->e_shoff;
    elf->strings = data + elf->sections[elf->header->e_shstrndx].sh_offset;
    elf->size = 0;

    /* section 0 is reserved */
    for (i = 1; i < elf->header->e_shnum; i++) {
	Elf_Shdr *shdr = &elf->sections[i];

	if (shdr->sh_type == SHT_SYMTAB) {
	    elf->symtab = &elf->sections[i];
	    elf->strtab = &elf->sections[elf->sections[i].sh_link];
	    elf->names = data + elf->strtab->sh_offset;
	}
    }

    elf_module_layout(elf);
    return 0;
}

size_t elf_module_get_size(elf_module_t *elf)
{
    return elf->size;
}

void elf_module_set_data(elf_module_t *elf, void *data)
{
    elf->data = data;
}

void *elf_module_get_data(elf_module_t *elf)
{
    return elf->data;
}

int elf_module_load(elf_module_t *elf, void *dest, elf_module_link_cbs_t *cbs)
{
    int i;
    int res;
    Elf_Shdr *shdr;

    elf->start = dest;

    for (i = 1; i < elf->header->e_shnum; i++) {
	shdr = &elf->sections[i];

	if (!(shdr->sh_flags & SHF_ALLOC))
	    continue;

	memcpy(elf_module_get_ptr(elf, shdr->sh_addr),
	    (void *) elf->header + shdr->sh_offset, shdr->sh_size);
    }

    res = elf_module_link(elf, cbs);
    if (res < 0)
	goto out;

    res = elf_module_reloc(elf);
out:
    return res;
}

void *elf_module_lookup_symbol(elf_module_t *elf, char *name)
{
    int i;
    int n = elf->symtab->sh_size / sizeof(Elf_Sym);
    Elf_Sym *sym = (void *) elf->header + elf->symtab->sh_offset + sizeof(Elf_Sym);

    for (i = 1; i < n; i++, sym++) {
	switch (sym->st_shndx) {
	case SHN_ABS:
	    break;

	case SHN_UNDEF:
	    break;

	default:
	    if (!strcmp(elf_module_sym_name(elf, sym->st_name), name))
		return (void *) sym->st_value;
	}
    }
    return NULL;
}
