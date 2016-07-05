#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/module.h>
#include <xdev/task.h>
#include <xdev/binfmt.h>
#include <libc.h>


extern void elf_module_register(void);
static module_t* mod_queue = NULL;



static int module_load(const char* name);

static int module_resolve_deps(module_t* mod) {
	if(mod->deps[0] == '\0')
		return E_OK;

	char* sp = strdup(mod->deps);
	char* s = sp;	
	char* p = 0;

	do {
		if((p = strchr(s, ',')))
			*p++ = 0;

		if(module_load(s) == E_ERR) {
			kprintf(ERROR, "module: cannot load dependency \"%s\" of \"%s\"\n", s, mod->name);
			kfree(sp);

			return E_ERR;
		}

		s = p;
	} while(s);

	kfree(sp);
	return E_OK;
}


static int module_load(const char* name) {
	module_t* tmp;
	for(tmp = mod_queue; tmp; tmp = tmp->next) {
		if(strcmp(tmp->name, name) != 0)
			continue;

		if(tmp->loaded)
			return E_OK;

		if(unlikely(module_resolve_deps(tmp) == E_ERR))
			continue;


		tmp->init();
		tmp->loaded = 1;

		return E_OK;
	}

	return E_ERR;
}


__optimize(fast)
int module_init(void) {

	elf_module_register();
	
	size_t size;
	register int i;
	for(i = 0; i < mbd->modules.count; i++) {
		
		size = mbd->modules.ptr[i].size;


		module_t* mod = (module_t*) binfmt_load_image((void*) mbd->modules.ptr[i].ptr, NULL, &size, "ELF_MODULE");
		if(unlikely(!mod)) {
			kprintf(ERROR, "module: cannot load module %d\n", i);
			continue;
		}

		mod->next = mod_queue;
		mod_queue = mod;

#if DEBUG
		kprintf(INFO, "module: loaded \"%s\" at %p\n", mod->name, mod->loaded_address);
#endif
	}


	module_t* tmp;
	for(tmp = mod_queue; tmp; tmp = tmp->next) {
		if(tmp->loaded)
			return E_OK;

		if(unlikely(module_resolve_deps(tmp) == E_ERR))
			continue;


		tmp->init();
		tmp->loaded = 1;
	}


	return E_OK;
}
