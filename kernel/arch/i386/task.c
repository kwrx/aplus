#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/intr.h>
#include <libc.h>

#include <arch/i386/i386.h>
#include "mm/paging.h"


#define FPU(x)		((void*) (((uintptr_t) (x) & 0x200) + 0x200))
#define CTX(x)		((i386_task_context_t*) x->context)


typedef struct i386_task_context {
	uintptr_t esp;
	uintptr_t ebp;
	uintptr_t eip;
	uintptr_t vmmpd;
	uint8_t fpu[1024];
	uint8_t sigstack[128];
} i386_task_context_t;

extern int end;



extern void return_from_fork(void);
extern void return_from_clone(void);
extern void return_from_clone_and_exit(void);
__asm__ (
	".globl return_from_fork	\n"
	"return_from_fork:			\n"
	"	pop gs					\n"
	"	pop fs					\n"
	"	pop es					\n"
	"	pop ds					\n"
	"	popa					\n"
	"	add esp, 8				\n"
	"	mov dx, 0x20			\n"
	"	mov al, 0x20			\n"
	"	out dx, al				\n"
	"	xor eax, eax			\n"
	"	sti						\n"
	"iret						\n"
);


__asm__ (
	".globl return_from_clone_and_exit		\n"
	"return_from_clone_and_exit:			\n"
	"	push eax							\n"
	"	call sys_exit						\n"
	".L0:									\n"
	"	jmp .L0								\n"
);

__asm__ (
	".globl return_from_clone	\n"
	"return_from_clone:			\n"
	"	mov dx, 0x20			\n"
	"	mov al, 0x20			\n"
	"	out dx, al				\n"
	"	sti						\n"
	"	pop eax					\n"
	"	jmp eax					\n"
);

extern char** args_dup(char**);

void fork_handler(i386_context_t* context) {
	INTR_OFF;

	volatile task_t* child = (volatile task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);
	KASSERT(child);

	memset((void*) child, 0, sizeof(task_t));

	child->pid = sched_nextpid();
	child->uid = current_task->uid;
	child->gid = current_task->gid;
	child->sid = current_task->sid;

	
	child->name = strdup(current_task->name);
	child->description = strdup(current_task->description);
	child->argv = args_dup(current_task->argv);
	child->environ = args_dup(current_task->environ);

	child->status = TASK_STATUS_READY;
	child->priority = current_task->priority;

	child->sig_handler = current_task->sig_handler;
	child->sig_no = current_task->sig_no;
	child->sig_mask = current_task->sig_mask;
	

	int i;
	for(i = 0; i < TASK_FD_COUNT; i++)
		memcpy((void*) &child->fd[i], (const void*) &current_task->fd[i], sizeof(fd_t));
	
	child->cwd = current_task->cwd;
	child->exe = current_task->exe;
	child->root = current_task->root;
	child->umask = current_task->umask;
	
	memcpy(&child->fifo, &current_task->fifo, sizeof(fifo_t));
	//memcpy(&child->iostat, &current_task->iostat, sizeof(current_task->iostat));
	memcpy(&child->clock, &current_task->clock, sizeof(struct tms));
	memcpy(&child->exit, &current_task->exit, sizeof(current_task->exit));
	memcpy(&child->__image, current_task->image, sizeof(child->__image));
	
	child->image = &child->__image;
		

	child->context = (void*) kmalloc(sizeof(i386_task_context_t), GFP_KERNEL);
	KASSERT(child->context);


	CTX(child)->eip = (uintptr_t) &return_from_fork;
	CTX(child)->esp = 
	CTX(child)->ebp = (uintptr_t) context;
	CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 1);


	char* sys_stack = (char*) kmalloc(CONFIG_STACK_SIZE, GFP_KERNEL);
	KASSERT(sys_stack);
	memcpy(sys_stack, current_task->sys_stack, CONFIG_STACK_SIZE);
	child->sys_stack = &sys_stack[CONFIG_STACK_SIZE];

	
	child->parent = (task_t*) current_task;
	child->next = (task_t*) task_queue;

	task_queue = child;


	context->eax = (uintptr_t) child;
	INTR_ON;
}


volatile task_t* arch_task_clone(int (*fn) (void*), void* stack, int flags, void* arg) {
	INTR_OFF;

	if(unlikely(!stack))
		stack = (void*) ((uintptr_t) kvalloc(PAGE_SIZE, GFP_USER) + PAGE_SIZE);

	uintptr_t* sptr = (uintptr_t*) stack;
	*--sptr = (uintptr_t) arg;
	*--sptr = (uintptr_t) &return_from_clone_and_exit;
	*--sptr = (uintptr_t) fn;

	

	volatile task_t* child = (volatile task_t*) kmalloc(sizeof(task_t), GFP_KERNEL);
	KASSERT(child);

	memset((void*) child, 0, sizeof(task_t));

	child->pid = sched_nextpid();
	child->uid = current_task->uid;
	child->gid = current_task->gid;
	child->sid = current_task->sid;

	
	child->name = strdup(current_task->name);
	child->description = strdup(current_task->description);
	child->argv = args_dup(current_task->argv);
	child->environ = args_dup(current_task->environ);

	child->status = TASK_STATUS_READY;
	child->priority = current_task->priority;

	
	if(flags & CLONE_SIGHAND) {
		child->sig_handler = current_task->sig_handler;
		child->sig_no = current_task->sig_no;
		child->sig_mask = current_task->sig_mask;
	}


	if(flags & CLONE_FILES) {
		int i;
		for(i = 0; i < TASK_FD_COUNT; i++)
			memcpy((void*) &child->fd[i], (const void*) &current_task->fd[i], sizeof(fd_t));
	}

	if(flags & CLONE_FS) {
		child->cwd = current_task->cwd;
		child->root = current_task->root;
		child->umask = current_task->umask;
	} else {
		child->cwd = kernel_task->cwd;
		child->root = kernel_task->root;
		child->umask = 022;
	}

	child->exe = current_task->exe;

	memcpy(&child->fifo, &current_task->fifo, sizeof(fifo_t));
	//memcpy(&child->iostat, &current_task->iostat, sizeof(current_task->iostat));
	memcpy(&child->clock, &current_task->clock, sizeof(struct tms));
	memcpy(&child->exit, &current_task->exit, sizeof(current_task->exit));


	child->context = (void*) kmalloc(sizeof(i386_task_context_t), GFP_KERNEL);
	KASSERT(child->context);


	CTX(child)->eip = (uintptr_t) &return_from_clone;
	CTX(child)->esp = 
	CTX(child)->ebp = (uintptr_t) sptr;


	char* sys_stack = (char*) kmalloc(CONFIG_STACK_SIZE, GFP_KERNEL);
	KASSERT(sys_stack);
	memcpy(sys_stack, current_task->sys_stack, CONFIG_STACK_SIZE);
	child->sys_stack = &sys_stack[CONFIG_STACK_SIZE];


	if(flags & CLONE_VM) {
		CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 0);
		child->image = current_task->image;
	} else {
		CTX(child)->vmmpd = (uintptr_t) vmm_clone((volatile pdt_t*) CTX(current_task)->vmmpd, 1);
		
		memcpy(&child->__image, current_task->image, sizeof(child->__image));
		child->image = &child->__image;
	}

	if(flags & CLONE_PARENT)
		child->parent = (task_t*) current_task->parent;
	else
		child->parent = (task_t*) current_task;
	

	child->next = (task_t*) task_queue;
	task_queue = child;

	INTR_ON;
	return child;
}



volatile task_t* arch_task_fork(void) {
	
	volatile task_t* r = NULL;
	__asm__ __volatile__ ("int 0x7F" : "=a"(r) : "a"(0));

	return r;
}


void arch_task_yield(void) {
	__asm__ __volatile__ ("int 0x7F" : : "a"(1));
}

void arch_task_switch(volatile task_t* prev_task, volatile task_t* new_task) {
	INTR_OFF;

	uintptr_t esp, ebp, eip;
	__asm__ __volatile__ ("mov eax, esp" : "=a"(esp));
	__asm__ __volatile__ ("mov eax, ebp" : "=a"(ebp));
	
	eip = read_eip();
	if(eip == 0) {
		if(unlikely(current_task->alarm > 0)) {
			if(likely(current_task->alarm <= timer_gettimestamp())) {
				current_task->sig_no = SIGALRM;
				current_task->alarm = 0;
			}
		}

		if(unlikely(
			(current_task->sig_no != 0) &&
			(current_task->sig_handler != NULL)
		)) {
			register int sig_no = current_task->sig_no;
			current_task->sig_no = 0;

			irq_ack(0);
			current_task->sig_handler(sig_no);
		}
		
		return;
	}

	CTX(prev_task)->eip = eip;
	CTX(prev_task)->esp = esp;
	CTX(prev_task)->ebp = ebp;

	__asm__ __volatile__ (
		"fsave [%0]	\n"
		"frstor [%1]	\n"
		: : "r"(FPU(CTX(prev_task)->fpu)), "r"(FPU(CTX(new_task)->fpu))
	);
	
	
	
	eip = CTX(new_task)->eip;
	esp = CTX(new_task)->esp;
	ebp = CTX(new_task)->ebp;

	x86_intr_kernel_stack((uintptr_t) new_task->sys_stack);



	volatile pdt_t* pd = (volatile pdt_t*) CTX(new_task)->vmmpd;

#if CONFIG_VMM
	vmm_switch(pd);
#endif

	__asm__ __volatile__ (
		"cli			\n"
		"mov ebx, %0	\n"
		"mov esp, %1	\n"
		"mov ebp, %2	\n"
		"mov cr3, %3	\n"
		"xor eax, eax	\n"
		"jmp ebx		\n"
		: : "r"(eip), "r"(esp), "r"(ebp), "r"(pd->physaddr)
		: "ebx", "esp", "eax"
	);	
}

void arch_task_release(volatile task_t* task) {
	KASSERT(task);
	KASSERT(task != kernel_task);

		
	int i;
	if(likely(current_task->argv)) {
		for(i = 0; current_task->argv[i]; i++)
			kfree(current_task->argv[i]);
			
		kfree(current_task->argv);
	}
		
	if(likely(current_task->environ)) {
		for(i = 0; current_task->environ[i]; i++)
			kfree(current_task->environ[i]);		
		
		kfree(current_task->environ);
	}


	vmm_release((volatile pdt_t*) CTX(task)->vmmpd);
}



int task_init(void) {
	static task_t __t;
	static i386_task_context_t __c;
	static char __sys_stack[CONFIG_STACK_SIZE];

	volatile task_t* t = task_queue = kernel_task = (volatile task_t*) &__t;

	KASSERT(t);
	memset((void*) t, 0, sizeof(task_t));

	t->pid = sched_nextpid();
	t->uid = TASK_ROOT_UID;
	t->gid = TASK_ROOT_GID;
	t->sid = 0;
	
	t->name = "aplus";
	t->description = NULL;
	t->argv = NULL;
	t->environ = NULL;
	
	t->status = TASK_STATUS_READY;
	t->priority = TASK_PRIO_REGULAR;
	
	t->sig_handler = NULL;
	t->sig_no = 0;
	t->sig_mask = 0;

	fifo_init(&t->fifo);

	int i;
	for(i = 0; i < TASK_FD_COUNT; i++)
		memset((void*) &t->fd[i], 0, sizeof(fd_t));
	
	t->cwd = NULL;
	t->exe = NULL;
	t->root = NULL;
	t->umask = 0;

	t->context = &__c;
	t->sys_stack = &__sys_stack[CONFIG_STACK_SIZE];
	CTX(t)->vmmpd = (uintptr_t) current_pdt;


	t->exit.status = 0;
	t->exit.value = 0;


	t->image = &t->__image;
	t->image->start = CONFIG_KERNEL_BASE;
	t->image->end = (uintptr_t) &end;

	t->parent = NULL;
	t->next = NULL;


	x86_intr_kernel_stack((uintptr_t) t->sys_stack);

	current_task = t;
	return E_OK;
}
