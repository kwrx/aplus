#ifdef __i386__

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/fs.h>
#include <aplus/list.h>
#include <errno.h>

#include "i386.h"

#if HAVE_DISASM
#include <libdis.h>

#define DISASM_RANGE		32
#endif


extern list_t* task_queue;
extern task_t* current_task;
extern task_t* kernel_task;


/**
 *	\brief Print last error number.
 */
static void dump_errno() {
	kprintf("Errno:\n\t%d - %s\n", errno, strerror(errno));
}



/**
 *	\brief Print current registers & cpu status.
 */
static void dump_cpu(regs_t* r) {
	kprintf("CPU:\n");

	if(r) {
		kprintf("eip: 0x%8x\n", r->eip);
		kprintf("err: 0x%8x\n", r->err_code);
		kprintf("int: 0x%8x\n", r->int_no);
		kprintf("esp: 0x%8x\n", r->esp);
		kprintf("usp: 0x%8x\n", r->useresp);
	}

	kprintf("tmr: %d ticks (%d Hz)\n", timer_getticks(), timer_getfreq());

#if HAVE_DISASM

	kprintf("Code:\n");

	x86_init(opt_none, NULL, NULL);
	x86_insn_t op;

	int eip = r->eip ;
	uint8_t* codebuf = (uint8_t*) eip;
	uint8_t* linebuf[256];
	
	int i, p;
	for(i = 0, p = 0; i < DISASM_RANGE; i++) {
		int s = x86_disasm(codebuf, 1024, eip, p, &op);
		if(likely(s)) {
			x86_format_insn(&op, (char*) linebuf, 256, intel_syntax);
			kprintf("%c %x:\t%s\n", op.addr == r->eip ? '>' : ' ', op.addr, linebuf);
			
			p += s;
		} else {
			kprintf("\t???\n");
			p++;
		}
	}

	x86_cleanup();
#endif

	kprintf("\n");
}


/**
 *	Print a stacktrace.
 */
static void dump_stacktrace(int count) {
	kprintf("Stack trace:\n");
	
	int* esp = NULL; 
	__asm__("mov eax, ebp" : "=a"(esp));


	for(int i = 0; i < count; i++) {
		int eip = *(++esp);
		char* sym = (char*) elf_symbol_lookup(eip);

		if(unlikely(!sym))
			break;

		kprintf("\t[%d] 0x%x - %s\n", i, eip, sym);
		

		esp = (int*) *(--esp);
		if(!esp)
			break;
	}

	kprintf("\n");
}


/**
 *	\brief Print all and current task.
 */
static void dump_task() {
	kprintf("Task:\n");
	
	list_foreach(value, task_queue) {
		task_t* task = (task_t*) value;
		kprintf("\t%d: ", task->pid);
		
		if(task->exe)
			kprintf("%s ", task->exe->name);
		else
			kprintf("unknown ");
			
		if(task == current_task)
			kprintf("(current) ");
			
		if(task == kernel_task)
			kprintf("(kernel) ");
			
		kprintf("\n");
	}
}


/**
 *	\brief Print MMU State.
 */
static void dump_mmu() {
	kprintf("Memory:\n");

	heap_t* h = (heap_t*) mm_getheap();

	kprintf("\tUsed: %d MB (%d Bytes)\n", (int)(h->used / 1024 / 1024), (int)h->used);
	kprintf("\tSize: %d MB (%d Bytes)\n", (int)(h->size / 1024 / 1024), (int)h->size);

}



/**
 *	\brief Go in Kernel Panic, dump exception registers, halt system.
 */
void arch_panic(char* msg, regs_t* r) {
	__asm__ ("cli");
	kprintf("\naplus: PANIC! \"%s\"\n", msg);
	
	
	
	dump_cpu(r);
	dump_task();
	dump_errno();
	dump_mmu();	
	dump_stacktrace(10);

	for(;;);
}

#endif
