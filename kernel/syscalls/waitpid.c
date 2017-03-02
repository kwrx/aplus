#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <libc.h>

struct wait {
	volatile task_t* task;
	struct wait* next;
};


static struct wait* __wait_for_pid(pid_t pid) {
	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		if(tmp->parent != current_task)
			continue;

		if(tmp->pid == pid) {
			struct wait* wx = (struct wait*) kmalloc(sizeof(struct wait), GFP_KERNEL);
			KASSERT(wx);
		
			wx->task = tmp;
			wx->next = NULL;

			return wx;
		}
	}

	errno = ESRCH;
	return NULL;
}

static struct wait* __wait_for_gid(gid_t gid) {

	struct wait* wq = NULL;
	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		if(tmp->parent != current_task)
			continue;

		if(tmp->gid == gid) {
			struct wait* wx = (struct wait*) kmalloc(sizeof(struct wait), GFP_KERNEL);
			KASSERT(wx);
		
			wx->task = tmp;
			wx->next = wq;
			wq = wx;
		}
	}

	if(unlikely(!wq))
		errno = ESRCH;

	return wq;
}

static struct wait* __wait_for_childs() {

	struct wait* wq = NULL;
	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		if(tmp->parent != current_task)
			continue;
		
		struct wait* wx = (struct wait*) kmalloc(sizeof(struct wait), GFP_KERNEL);
		KASSERT(wx);
		
		wx->task = tmp;
		wx->next = wq;
		wq = wx;
	}

	if(unlikely(!wq))
		errno = ESRCH;

	return wq;
}


SYSCALL(31, waitpid,
pid_t sys_waitpid(pid_t pid, int* status, int options) {

	struct wait* waiters = NULL;

	if(pid > 0)
		waiters = __wait_for_pid(pid);

	if(pid == 0)
		waiters = __wait_for_gid(current_task->gid);

	if(pid < -1)
		waiters = __wait_for_gid(-pid);

	if(pid == -1)
		waiters = __wait_for_childs();

	if(unlikely(!waiters))
		return -1;




	struct wait* tmp;
	pid_t cnt = -1;

	do {
		for(tmp = waiters; tmp; tmp = tmp->next) {
	
			if(tmp->task->status == TASK_STATUS_KILLED) {
				if(likely(status))
					*status = (tmp->task->exit.status << 16) | tmp->task->exit.value;
				cnt = tmp->task->pid;
				
				break;
			}
		}

		sys_yield();
	} while(cnt == -1);


	struct wait* t2;
	for(tmp = waiters; tmp;) {
		t2 = tmp;
		tmp = tmp->next;

		kfree(t2);
	}

	return cnt;
});
