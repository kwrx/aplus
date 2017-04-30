#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <aplus/intr.h>
#include <libc.h>

SYSCALL(0, exit,
__attribute__((noreturn))
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
		

	arch_task_release(current_task);
	syscall_ack();

	INTR_ON;
	for(;;) 
		sys_yield();
});
