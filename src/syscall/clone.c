#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <errno.h>

extern task_t* current_task;


/**
 *	\brief Creates a new process.\n
 * 	Unlike fork(2), these calls allow the child process to share parts of its execution
	context with the calling process, such as the memory space, the table of file 
	descriptors, and the table of signal handlers.\n
	The main use of sys_clone() is to implement threads: multiple threads of control 
	in a program that run concurrently in a shared memory space.
 *	\param fn Is a pointer to a function that is called by the child process at the beginning of its execution.
 *	\param child_stack specifies the location of the stack used by the child process.\n
			Since the child and calling process may share memory, it is not possible 
			for the child process to execute in the same stack as the calling process.
 *	\param flags \n
	+	CLONE_FILES\n
			The calling process and the child process share the same file descriptor table.
	+	CLONE_SIGHAND\n
			The calling process and the child process share the same table of signal handlers.
	+	#CLONE_VM\n
			The calling process and the child process run in the same memory space.
 *	\param arg Argument is passed to the fn function.
 *	\return On success, the thread ID of the child process is returned in the caller's thread of execution.\n
			On failure, -1 is returned in the caller's context, no child process will be created,
			and errno will be set appropriately.
 */
int sys_clone(int (*fn)(void*), void* child_stack, int flags, void* arg) {
	if(!current_task)
		return -1;
	
	if(fn == NULL) {
		errno = EINVAL;
		return -1;
	}


	task_t* child = (task_t*) task_clone(fn, arg, child_stack, flags);
	if(!child) {
		errno = EFAULT;
		return -1;
	}

	return child->pid;
}

SYSCALL(sys_clone, 22);
