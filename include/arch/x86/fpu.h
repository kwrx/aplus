

#ifndef _APLUS_X86_FPU_H
#define _APLUS_X86_FPU_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>


__BEGIN_DECLS

void fpu_init(uint64_t);
void fpu_switch(void*, void*);
void fpu_save(void* fpu_area);
void fpu_restore(void* fpu_area);
void* fpu_new_state(void);
void fpu_free_state(void*);
size_t fpu_size(void);

__END_DECLS

#endif
#endif