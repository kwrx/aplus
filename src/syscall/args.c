#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;


char** sys__get_argv() {
	if(!current_task)
		return NULL;
		
	return current_task->argv;
}

char** sys__get_envp() {
	if(!current_task)
		return NULL;
		
	return current_task->envp;
}


SYSCALL(sys__get_argv, 100);
SYSCALL(sys__get_envp, 101);
