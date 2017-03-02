#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <libc.h>

#include <arch/i386/i386.h>


void pagefault_handler(i386_context_t* context) {
	if(unlikely(current_task == kernel_task)) {
		uintptr_t p;
		__asm__ ("mov eax, cr2" : "=a"(p));
		
		kprintf(ERROR, "Exception! Page Fault at address %p\n\t (PID: %d, PC: %p, SP: %p)\n", p, sys_getpid(), context->eip, context->esp);
	

		__asm__ ("cli");
		for(;;) __asm__("hlt");
	}
	

	__asm__("sti");
	sys_kill(current_task->pid, SIGSEGV);
}
