#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(12, sbrk,
void* sys_sbrk(ptrdiff_t incr) {
    if(current_task->image->end + incr < current_task->image->start) {
        errno = EINVAL;
        return NULL;
    }

    uintptr_t cr = current_task->image->end;

    incr += PAGE_SIZE;
    incr &= ~(PAGE_SIZE - 1);
    
    
    int i;
    if(incr > 0)
        for(i = 0; i < incr; i += PAGE_SIZE)
            map_page(current_task->image->end + i, pmm_alloc_frame() << 12, 1);
    else
        for(i = 0; i >= incr; i -= PAGE_SIZE)
            unmap_page(current_task->image->end + i);
    
    current_task->image->end += incr;
    current_task->vmsize += incr;
    return (void*) cr;
});
