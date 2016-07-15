#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/mm.h>
#include <xdev/ipc.h>
#include <xdev/timer.h>
#include <xdev/debug.h>
#include <xdev/syscall.h>
#include <libc.h>

typedef struct {
    uint8_t buffer[BUFSIZ];
    int w_pos;
    int r_pos;
	mutex_t w_lock;
	mutex_t r_lock;
} fifo_t;


static int fifo_read(struct inode* inode, void* ptr, size_t len) {
	if(unlikely(!inode || !ptr)) {
		errno = EINVAL;
		return E_ERR;
	}
	
	if(unlikely(!inode->userdata))
		return E_ERR;
		
	if(unlikely(!len))
		return 0;
		
	if(len > BUFSIZ)
		len = BUFSIZ;
		
	fifo_t* fifo = (fifo_t*) inode->userdata;
	register uint8_t* buf = (uint8_t*) ptr;
	
	mutex_lock(&fifo->r_lock);
	
	int i;
	for(i = 0; i < len; i++) {
		while(!(fifo->w_pos > fifo->r_pos))
			sys_yield();
			
		*buf++ = fifo->buffer[fifo->r_pos++ % BUFSIZ];
	}
	
	mutex_unlock(&fifo->r_lock);
	return len;			
}

static int fifo_write(struct inode* inode, void* ptr, size_t len) {
	if(unlikely(!inode || !ptr)) {
		errno = EINVAL;
		return E_ERR;
	}
	
	if(unlikely(!inode->userdata))
		return E_ERR;
		
	if(unlikely(!len))
		return 0;
	
		
	if(len > BUFSIZ)
		len = BUFSIZ;
		
		
	fifo_t* fifo = (fifo_t*) inode->userdata;
	register uint8_t* buf = (uint8_t*) ptr;
	
	
	mutex_lock(&fifo->w_lock);
	
	int i;
	for(i = 0; i < len; i++)
		fifo->buffer[(int) fifo->w_pos++ % BUFSIZ] = *buf++;
		
	mutex_unlock(&fifo->w_lock);
	return len;
}


SYSCALL(30, mkfifo,
int sys_mkfifo(const char* pathname, mode_t mode) {
	int fd = sys_open(pathname, O_RDWR | O_CREAT | O_EXCL, S_IFIFO | mode);
	if(unlikely(fd < 0))
		return -1;
		
	
	fifo_t* fifo = (fifo_t*) kmalloc(sizeof(fifo_t), GFP_USER);
	if(unlikely(!fifo)) {
		errno = ENOMEM;
		return -1;
	}
	
	
	fifo->w_pos =
	fifo->r_pos = 0;
	
	mutex_init(&fifo->r_lock, MTX_KIND_DEFAULT);
	mutex_init(&fifo->w_lock, MTX_KIND_DEFAULT);
	
		
	inode_t* inode = current_task->fd[fd].inode;
	inode->read = fifo_read;
	inode->write = fifo_write;
	inode->userdata = (void*) fifo;
	
	sys_close(fd);
	return 0;
});
