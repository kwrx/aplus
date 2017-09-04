#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <libc.h>

#include <arch/i386/i386.h>


void pagefault_handler(i386_context_t* context) {
    uintptr_t p;
    __asm__ ("mov eax, cr2" : "=a"(p));


    debug_dump(context, "Exception! Page Fault occured!", p, context->err_code);
    
    if(unlikely(current_task == kernel_task)) {
        __asm__ ("cli");
        for(;;) __asm__("hlt");
    }

    
        
    

    __asm__("sti");
    sys_kill(current_task->pid, SIGSEGV);
    sys_yield();
    sys_exit(SIGSEGV);
}
