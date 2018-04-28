#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/timer.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <libc.h>



static int inode_fifo_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
    (void) pos;
    
    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata))
        return -1;


    fifo_t* fifo = (fifo_t*) inode->userdata;
    return fifo_read(fifo, ptr, len);        
}

static int inode_fifo_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
    (void) pos;

    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata))
        return -1;
        
        
    fifo_t* fifo = (fifo_t*) inode->userdata;
    return fifo_write(fifo, ptr, len);
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
    

    fifo_init(fifo);
    
#if CONFIG_IPC_DEBUG
    #define mtxname strdup(pathname)
#else
    #define mtxname NULL
#endif
    
    mutex_init(&fifo->r_lock, MTX_KIND_DEFAULT, mtxname);
    mutex_init(&fifo->w_lock, MTX_KIND_DEFAULT, mtxname);
    
        
    inode_t* inode = current_task->fd[fd].inode;
    inode->read = inode_fifo_read;
    inode->write = inode_fifo_write;
    inode->userdata = (void*) fifo;
    
    sys_close(fd);
    return 0;
});
