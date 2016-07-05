#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <libc.h>

#include <arch/i386/i386.h>


void pagefault_handler(i386_context_t* context) {
	uintptr_t p;
	__asm__ ("mov eax, cr2" : "=a"(p));


	kprintf(ERROR, "Exception! Page Fault at address %p\n\t (PID: %d, PC: %p, SP: %p)\n", p, sys_getpid(), context->eip, context->esp);
	

	__asm__ ("cli");
	for(;;);
}
