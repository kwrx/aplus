#ifndef _MODULE_H
#define _MODULE_H


#define MODULE_NAME(x)	\
	const char* __module_name__ = x

#define MODULE_DEPS(x)	\
	const char* __module_deps__ = x


#define MODULE_AUTHOR(x);
#define MODULE_LICENSE(x);


#ifdef KERNEL

typedef struct module {
	const char* name;
	const char* deps;
	int (*init) (void);
	int (*dnit) (void);
	
	uintptr_t loaded_address;
	int loaded;
} module_t;

int module_init(void);
#endif

#endif
