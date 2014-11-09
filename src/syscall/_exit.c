#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


/**
 * 	\brief Terminates the process normally by returning control to the host environment, 
			but without performing any of the regular cleanup tasks for terminating
			processes (as function exit does).\n
			No object destructors, nor functions registered by atexit or at_quick_exit
			are called.
 *	\param status Status code.\n
			If this is 0 or EXIT_SUCCESS, it indicates success.\n
			If it is EXIT_FAILURE, it indicates failure.\n
 */
void sys_exit(int status) {
	schedule_exit(status);
}


SYSCALL(sys_exit, 0);
