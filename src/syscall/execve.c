#include <aplus.h>
#include <aplus/task.h>
#include <aplus/syscall.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


extern task_t* current_task;


static char** __args_dup(char** a) {
	if(!a)
		return NULL;

	char** p = (char**) kmalloc(255 * sizeof(char*));
	int i = 0;

	while(a[i]) {
		p[i] = (char*) kmalloc(strlen(a[i]));
		strcpy(p[i], a[i]);

		i++;
	}

	return p;
}



/**
 *	\brief Executes the program pointed to by filename.\n
		filename must be either a binary executable, or a script starting with a line of the form.\n
			#! interpreter [optional-arg]
 *	\param filename executable path.
 *	\param argv Is an array of argument strings passed to the new program.\n
		By convention, the first of these strings should contain the filename associated with the 
		file being executed.
 *	\param environ Is an array of strings, conventionally of the form key=value, which are passed
		as environment to the new program.
 *	\warning Both argv and environ must be terminated by a null pointer.
 *	\return Does not return on success, and the text, data, bss, and stack of the calling process	
		are overwritten by that of the program loaded.
 */
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

	
	int vaddr, vsize;
	void (*entry) () = (void (*) ()) elf32_load(image, &vaddr, &vsize);
	if(entry) {

		argv = __args_dup(argv);
		environ = __args_dup(environ);

		current_task->exe = current_task->fd[fd];
		current_task->argv = argv;
		current_task->envp = environ;

		current_task->image.ptr = (int) image;
		current_task->image.vaddr = vaddr;
		current_task->image.length = vsize;

		sys_close(fd);


		entry(); /* never return */
	}

	sys_close(fd);
	kfree(image);
	return -1;
}


SYSCALL(sys_execve, 2);
