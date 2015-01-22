#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/netif.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;


int sys_bind(int fd, struct sockaddr* addr, size_t size) {
	if(fd > TASK_MAX_FD) {
		errno = EBADF;
		return -1;
	}

	inode_t* ino = current_task->fd[fd];

	if(unlikely(!ino || !ino->userdata)) {
		errno = EBADF;	
		return -1;
	}

	
	netif_socket_t* sock = (netif_socket_t*) ino->userdata;
	return netif_socket_bind(sock, addr, size);
}



SYSCALL(sys_bind, 37);
