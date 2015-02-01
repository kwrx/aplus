#ifdef __rpi__

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/fs.h>
#include <aplus/list.h>
#include <errno.h>

#include "rpi.h"


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

void arch_panic(char* msg, regs_t* r) {
	kprintf("panic: %s\n", msg);

	dump_task();
	dump_errno();
	dump_mmu();	


	kprintf("System halted!\n");
	for(;;);
}

#endif
