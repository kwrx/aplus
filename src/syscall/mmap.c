#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
//#include <sys/mman.h>
#include <dirent.h>

extern task_t* current_task;

void* sys_mmap(void* addr, size_t length /*, int prot */, int flags, int fd, off_t offset) {
	return NULL;
}

int sys_umap(void* addr, size_t length) {
	errno = ENOSYS;	
	return -1;
}



SYSCALL(sys_mmap, 34);
SYSCALL(sys_umap, 35);
