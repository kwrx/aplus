#include <aplus.h>

#if HAVE_NETWORK

#include <aplus/spinlock.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/tcpip.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>

#include "lwip/opt.h"

#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcpip.h"
#include "lwip/pbuf.h"
#include "lwip/netdb.h"



extern task_t* current_task;

tcpip_t tcpip = {
	lwip_accept,
	lwip_bind,
	lwip_shutdown,
	lwip_getpeername,
	lwip_getsockname,
	lwip_getsockopt,
	lwip_setsockopt,
	lwip_close,
	lwip_connect,
	lwip_listen,
	lwip_recv,
	lwip_read,
	lwip_recvfrom,
	lwip_send,
	lwip_sendto,
	lwip_socket,
	lwip_write,
	lwip_select,
	lwip_ioctl,
	lwip_fcntl
};


static int sock_read(inode_t* ino, char* ptr, int size) {
	if(unlikely(!ino))
		return 0;

	return lwip_read((int) ino->userdata, (void*) ptr, (size_t) size);
}


static int sock_write(inode_t* ino, char* ptr, int size) {
	if(unlikely(!ino))
		return 0;

	return lwip_write((int) ino->userdata, (void*) ptr, (size_t) size);
}

static int sock_ioctl(inode_t* ino, int req, void* buf) {
	if(unlikely(!ino))
		return 0;

	return lwip_ioctl((int) ino->userdata, req, buf);
}

static void sock_flush(inode_t* ino) {
	if(unlikely(!ino))
		return;

	lwip_close((int) ino->userdata);
}

static int sock_open(int domain, int type, int proto) {
	static uint32_t sockid = 0;
	
	int sd = lwip_socket(domain, type, proto);
	if(sd < 0)
		return -1;

	char sockname[32];
	memset(sockname, 0, sizeof(sockname));
	ksprintf(sockname, "/tmp/sock%d", sockid++);
	
	int fd = sys_open(sockname, O_RDWR, S_IFCHR);
	if(fd < 0)
		return -1;

	inode_t* ino = current_task->fd[fd];
             
	ino->read = sock_read;
	ino->write = sock_write;
	ino->ioctl = sock_ioctl;
	ino->flush = sock_flush;

	ino->userdata = (void*) sd;

	return fd;
}


static int __tcpip_ioctl(inode_t* ino, int req, void* buf) {
	if(unlikely(!ino)) {
		errno = EINVAL;
		return -1;
	}

	switch(req) {
		case TCPIPIO_GETSOCK: {
			if(unlikely(current_task->fd[*(int*) buf] == NULL)) {
				errno = EBADF;
				return -1;
			}

			return (int) ((inode_t*) current_task->fd[*(int*) buf])->userdata;		
		} break;

		case TCPIPIO_OPENSOCK: {
			int* args = (int*) buf;
			return sock_open(args[0], args[1], args[2]);
		} break;

		case TCPIPIO_GETTABLE:
			if(unlikely(!buf)) {
				errno = EINVAL;
				return -1;
			}

			memcpy(buf, &tcpip, sizeof(tcpip_t));
			return 0;

		default:
			errno = EINVAL;
			return -1;
	}

	errno = EINVAL;
	return -1;
}

#endif

#define __p(x)	printf(#x ": %d\n", (x))
#define TEST_TCP 1
int writed = 0;
int ready = 0;

void client(void* unused) {
	while(!ready)
		sys_yield();
	
	kprintf("TRY CONNECT...\n");

	struct hostent* h;
	struct sockaddr_in cin;
	cin.sin_family = AF_INET;
	cin.sin_port = __builtin_bswap16(5000);
	
	h = lwip_gethostbyname("127.0.0.1");
	if(!h) {
		kprintf("CANNOT GET HOST\n");
		_exit(0);
	}
	memcpy(&cin.sin_addr, h->h_addr, h->h_length);

	
	int sp;
#if TEST_TCP
	__p(sp = lwip_socket(AF_INET, SOCK_STREAM, 0));
#else
	__p(sp = lwip_socket(AF_INET, SOCK_DGRAM, 0));
#endif


	__p(lwip_connect(sp, (struct sockaddr*) &cin, sizeof(cin)));
	
	kprintf("CLIENT CONNECTED!\n");

	char* buf = "Hello!!";
	kprintf("WRITING\n");
#if TEST_TCP
	__p(lwip_write(sp, buf, strlen(buf)));
#else
	__p(lwip_send(sp, buf, strlen(buf), 0));
#endif

	writed = 1;

	for(;;);
}


int init() {
#if HAVE_NETWORK
	inode_t* ino = (inode_t*) devfs_makedevice("tcpip", S_IFCHR);
	if(!ino) {
		kprintf("tcpip: cannot create device\n");
		return -1;
	}

	ino->ioctl = __tcpip_ioctl;
#endif


	struct sockaddr_in sin, cin;
	socklen_t scin = sizeof(cin);

	sin.sin_family = AF_INET;
	sin.sin_port = __builtin_bswap16(5000);
	sin.sin_addr.s_addr = INADDR_ANY;

	__p(sys_clone(client, NULL, __CLONE_THREAD, NULL));

	char buf[32];
	int sd, sc;
#if TEST_TCP
	__p(sd = lwip_socket(AF_INET, SOCK_STREAM, 0));
#else
	__p(sd = lwip_socket(AF_INET, SOCK_DGRAM, 0));
#endif
	__p(lwip_bind(sd, (struct sockaddr*) &sin, sizeof(sin)));
#if TEST_TCP
	__p(lwip_listen(sd, 5));
#endif

	ready = 1;

#if TEST_TCP
	__p(sc = lwip_accept(sd, (struct sockaddr*) &cin, &scin));
#endif

	while(!writed)
		sys_yield();
	kprintf("READING\n");
#if TEST_TCP
	__p(lwip_read(sc, buf, 32));
#else
	__p(lwip_recv(sc, buf, 32, 0));
#endif
	kprintf("MSG: %s\n", buf);


	return 0;
}


int dnit() {
	return 0;
}

