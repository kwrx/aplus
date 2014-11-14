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


int sys__install_sighandler(void* handler) {
	if(!current_task)
		return -1;
		
	current_task->signal_handler = (void (*)(int)) handler;
	return 0;
}



SYSCALL(sys__install_sighandler, 102);
