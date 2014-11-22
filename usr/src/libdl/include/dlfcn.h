#ifndef _DLFCN_H
#define _DLFCN_H

#define RTLD_NEXT			((void*) -1l)
#define RTLD_DEFAUL			((void*) 0)

#define RTLD_LAZY			0x0001
#define RTLD_NOW			0x0002
#define RTLD_BINDING_MASK	0x0003
#define RTLD_NOLOAD			0x0004
#define RTLD_DEEPBIND		0x0008
#define RTLD_GLOBAL			0x0100
#define RTLD_LOCAL			0x0000
#define RTLD_NODELETE		0x1000

typedef struct {
	const char* dli_fname;
	void* dli_fbase;
	const char* dli_sname;
	void* dli_saddr;
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif

extern void* dlopen(const char* file, int mode);
extern int dlclose(void* handle);
extern void* dlsym(void* handle, const char* name);
extern char* dlerror(void);


#ifdef __cplusplus
}
#endif

#endif
