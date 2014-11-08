#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/list.h>
#include <errno.h>


extern list_t* task_queue;
extern task_t* current_task;
extern task_t* kernel_task;


static void dump_errno() {
	kprintf("errno: %d - %s\n", errno, strerror(errno));
}

static void dump_registers() {

	kprintf("Registers:\n");

	#define d(reg);								\
		kprintf(#reg ": 0x%x\t\t", read_##reg());
		
	#define _d();								\
		kprintf("\n");	
			
	d(eax);
	d(ebx);
	d(ecx);
	d(edx);
	d(esi);
	d(edi);
	_d();
	d(eip);
	d(eflags);
	d(esp);
	d(ebp);
	_d();
	d(cs);
	d(ds);
	d(es);
	d(fs);
	d(gs);
	_d();
	d(cr0);
	d(cr2);
	d(cr3);
	d(cr4);
	_d();
	_d();
	
	#undef d
	#undef _d
}


static void dump_stacktrace(int count) {
	kprintf("Stack trace:\n");
	kprintf("TODO\n\n");
}

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

void panic(char* msg) {
	__asm__ ("cli");
	kprintf("panic: \"%s\"\n", msg);
	
	
	dump_registers();
	dump_stacktrace(6);
	dump_task();
	dump_errno();
	
	for(;;);
}
