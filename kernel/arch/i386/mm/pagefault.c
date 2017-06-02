#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <libc.h>

#include <arch/i386/i386.h>


void pagefault_handler(i386_context_t* context) {
	uintptr_t p;
	__asm__ ("mov eax, cr2" : "=a"(p));


	#define lookup(s, a)														\
		!(s = binfmt_lookup_symbol(current_task->image->symtab, a))				\
			? !(s = binfmt_lookup_symbol(kernel_task->image->symtab, a))		\
				? s = "<unknown>" : (void) 0 : (void) 0;


	char* fsym, *esym;
	lookup(fsym, p);
	lookup(esym, context->eip);
	
	kprintf(ERROR "Exception! Page Fault occured on %d\n"
				  "\t Address: %p (%s)\n"
				  "\t PC: %p (%s)\n"
				  "\t SP: %p\n",
				  sys_getpid(), p, fsym, context->eip, esym, context->esp); 

	if(unlikely(current_task == kernel_task)) {
		__asm__ ("cli");
		for(;;) __asm__("hlt");
	}

	
		
	

	__asm__("sti");
	sys_kill(current_task->pid, SIGSEGV);
	sys_yield();
	sys_exit(SIGSEGV);
}
