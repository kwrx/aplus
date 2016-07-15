#include <xdev.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/debug.h>
#include <xdev/intr.h>
#include <libc.h>

SYSCALL(0, exit,
void sys_exit(int status) {
	KASSERT(current_task != kernel_task);

	INTR_OFF;

	current_task->status = TASK_STATUS_KILLED;
	current_task->exit.status = TASK_EXIT_EXITED;
	current_task->exit.value = status & 0xFFFF;

	if(current_task == task_queue)
		task_queue = current_task->next;
	else {
		volatile task_t* tmp;
		for(tmp = task_queue; tmp; tmp = tmp->next) {
			if(tmp->next == current_task)
				tmp->next = current_task->next;
		}
	}


	int i;
	for(i = 0; i < TASK_FD_COUNT; i++)
		sys_close(i);
		
	for(i = 0; current_task->argv[i]; i++)
		kfree(current_task->argv[i]);
		
	for(i = 0; current_task->environ[i]; i++)
		kfree(current_task->environ[i]);
		
	kfree(current_task->argv);
	kfree(current_task->environ);


	arch_task_release(current_task);


	INTR_ON;
	for(;;) sys_yield();
});
