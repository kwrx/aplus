#ifndef _ELF_MODULE_H
#define _ELF_MODULE_H

#include <sys/types.h>

struct elf_ehdr;
struct elf_shdr;

/*
 * Error codes.
 */
enum {
    EME_OK = 0,
    EME_NOEXEC = 2,
    EME_UNSUPPORTED = 3,
    EME_UNDEFINED_REFERENCE = 4,
    EME_MULTIPLE_DEFINITIONS = 5,
};


typedef struct elf_symbol {
	struct elf_symbol* next;
	void* addr;
	char name[0];
} elf_symbol_t;

/*
 * ELF module descriptor.
 */
typedef struct {
    struct elf_ehdr *header;
    struct elf_shdr *sections;
    struct elf_shdr *symtab;
    struct elf_shdr *strtab;

    char *names;
    char *strings;

    size_t size;
    void *start;

	elf_symbol_t* e_symtab;
    void *data; /* user data */
} elf_module_t;

/*
 * Linker callbacks
 */
typedef struct {
    void *(*resolve)(elf_module_t *elf, char *symbol);
    int (*define)(elf_module_t *elf, char *symbol, void *addr);
} elf_module_link_cbs_t;

/*
 * Assign given elf_module_t structure
 */
extern int elf_module_init(elf_module_t *elf, void *image, size_t size);

/*
 * Get size of memory needed to load code and data
 */
extern size_t elf_module_get_size(elf_module_t *elf);

/*
 * Set user data for given ELF module
 */
extern void elf_module_set_data(elf_module_t *elf, void *data);

/*
 * Get user data for given ELF module
 */
extern void *elf_module_get_data(elf_module_t *elf);

/*
 * Loads module's sections to given location, callbacks are used
 * to link module.
 */
extern int elf_module_load(elf_module_t *elf, void *dest, elf_module_link_cbs_t *cb);

/*
 * Lookup symbol in given module.
 */
extern void *elf_module_lookup_symbol(elf_module_t *module, char *sym);

#endif /* _ELF_H */
