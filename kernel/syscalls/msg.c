#include <aplus.h>
#include <aplus/base.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/msg.h>
#include <libc.h>


/* MSGHDR       */
/*  0-3: pid    */
/*  4-7: size   */
/*  8-N: data   */


SYSCALL(105, msg_send,
int sys_msg_send(pid_t pid, void* data, size_t len) {
	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		if(tmp->pid == pid) {
			if(tmp->sig_mask & (1 << SIGMSG)) {
				errno = EPERM;
				return 0;
			}

            pid = getpid();
            if(fifo_write(&tmp->fifo, &pid, sizeof(pid_t)) != sizeof(pid_t)) {
                errno = EIO;
                return 0;
            }

            if(fifo_write(&tmp->fifo, &len, sizeof(size_t)) != sizeof(size_t)) {
                errno = EIO;
                return 0;
            }

            if(fifo_write(&tmp->fifo, data, len) != len) {
                errno = EIO;
                return 0;
            }


			tmp->sig_no = SIGMSG;
			return len;
		}
	}

	errno = ESRCH;
	return 0;
});

SYSCALL(106, msg_recv,
int sys_msg_recv(pid_t* pid, void* data, size_t len) {
    if(fifo_available(&current_task->fifo) < ((sizeof(pid_t) + sizeof(size_t)))) {
        errno = EAGAIN;
        return 0;
    }

	if(fifo_read(&current_task->fifo, pid, sizeof(pid_t)) != sizeof(pid_t)) {
        errno = EIO;
        return 0;
    }

    size_t l;
    if(fifo_read(&current_task->fifo, &l, sizeof(size_t)) != sizeof(size_t)) {
        errno = EIO;
        return 0;
    }


    static char buf[BUFSIZ];
    if(fifo_read(&current_task->fifo, buf, l) != l) {
        errno = EIO;
        return 0;
    }

    if(l > len)
        l = len;
    memcpy(data, buf, l);

    return l;
});
