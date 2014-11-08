#include <aplus.h>
#include <aplus/task.h>
#include <aplus/syscall.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


extern task_t* current_task;

int sys_execve(char* filename, char** argv, char** environ) {
	
	int fd = sys_open(filename, O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return -1;
	}

	sys_lseek(fd, 0, SEEK_END);
	size_t size = sys_lseek(fd, 0, SEEK_CUR);
	sys_lseek(fd, 0, SEEK_SET);

	void* image = kmalloc(size);
	if(!image) {
		sys_close(fd);

		errno = ENOMEM;
		return -1;
	}

	if(sys_read(fd, image, size) < size) {
		sys_close(fd);
		kfree(image);
		
		errno = EIO;
		return -1;
	}

	

	void (*entry) () = (void (*) ()) elf32_load(image);
	if(entry) {

		current_task->exe = current_task->fd[fd];
		current_task->argv = argv;
		current_task->envp = environ;

		sys_close(fd);


		entry(); /* never return */
	}

	sys_close(fd);
	kfree(image);
	return -1;
}


SYSCALL(sys_execve, 2);
