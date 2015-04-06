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


static int __sock_read(inode_t* ino, char* buf, int size) {
	if(unlikely(!ino || !buf || !size || !ino->userdata)) {
		errno = EINVAL;
		return 0;
	}


	netif_socket_t* sock = (netif_socket_t*) ino->userdata;
	int i = 0, s = size;

	while(i < size) {
		int c = netif_socket_read(sock, (void*) ((uint32_t) buf + i), s);

		if(unlikely(!c))
			return i;
		
		i += c;
		s -= c;
	}

	return i;
}

static int __sock_write(inode_t* ino, char* buf, int size) {
	if(unlikely(!ino || !buf || !size || !ino->userdata)) {
		errno = EINVAL;
		return 0;
	}


	netif_socket_t* sock = (netif_socket_t*) ino->userdata;
	int i = 0, s = size;

	while(i < size) {
		int c = netif_socket_write(sock, (void*) ((uint32_t) buf + i), s);

		if(unlikely(!c))
			return i;
		
		i += c;
		s -= c;
	}

	return i;
}


int sys_socket(int domain, int type, int proto) {
	
	netif_socket_t* sock = (netif_socket_t*) netif_socket_create(domain, type, proto);
	if(unlikely(!sock)) {
		errno = EINVAL;
		return -1;
	}

	char buf[128];
	memset(buf, 0, 128);

	ksprintf(buf, "/tmp/sock%d", (int) sock->id);

	int fd = sys_open(buf, O_CREAT | O_EXCL | O_TRUNC, S_IFSOCK);
	if(fd < 0) {
		netif_socket_close(sock, 2);
		kfree(sock);
	
		return -1;
	}
	
	inode_t* ino = current_task->fd[fd];
	ino->write = __sock_write;
	ino->read = __sock_read;
	ino->userdata = (void*) sock;

	return fd;
}



SYSCALL(sys_socket, 36);
