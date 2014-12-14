#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/fs.h>
#include <aplus/list.h>
#include <errno.h>


extern list_t* task_queue;
extern task_t* current_task;
extern task_t* kernel_task;


/**
 *	\brief Print last error number.
 */
static void dump_errno() {
	kprintf("errno: %d - %s\n", errno, strerror(errno));
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

	kprintf("pit: %dms\n", pit_getticks());

	kprintf("\n\n");
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
		kprintf("[%d] 0x%x - %s\n", i, eip, elf_symbol_lookup(eip));
		

		esp = (int*) *(--esp);
		if(!esp)
			break;
	}

	kprintf("\n\n");
}


/**
 *	\brief Print all and current task.
 */
static void dump_task() {
	kprintf("Task:\n");
	
	list_foreach(value, task_queue) {
		task_t* task = (task_t*) value;
		kprintf(" # %d: ", task->pid);
		
		if(task->exe)
			kprintf("%s ", task->exe->name);
		else
			kprintf("unknown ");
			
		if(task == current_task)
			kprintf("(current) ");
			
		if(task == kernel_task)
			kprintf("(kernel) ");
			
		kprintf("\n\n");
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

	kprintf("\n\n");
}



/**
 *	\brief Go in Kernel Panic, dump exception registers, halt system.
 */
void panic_r(char* msg, regs_t* r) {
	__asm__ ("cli");
	kprintf("panic: \"%s\"\n", msg);
	
	
	
	dump_cpu(r);

	dump_stacktrace(6);
	dump_task();
	dump_errno();
	dump_mmu();	

	for(;;);
}

/**
 *	\brief Go in Kernel Panic, halt system.
 */
void panic(char* msg) {
	panic_r(msg, NULL);
}


