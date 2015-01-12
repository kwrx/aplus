#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/list.h>
#include <aplus/tty.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/termios.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/stat.h>

extern task_t* current_task;
extern task_t* kernel_task;

list_t* ttydevs = NULL;

cc_t ttydefchars[NCCS] = {
	//CEOF, CEOL, CEOL, CERASE, CWERASE, CKILL, CREPRINT, 
	//_POSIX_VDISABLE, CINTR, CQUIT, CSUSP, CDSUSP, CSTART, CSTOP, CLNEXT,
	//CDISCARD, CMIN, CTIME, CSTATUS, _POSIX_VDISABLE
};


static void __default_tty_output(tty_t* tty, void* ptr, size_t size) {
#ifdef DEBUG
	char* ctr = (char*) ptr;
	
	for(int i = 0; i < size; i++)
		debug_putc(ctr[i]);
#endif
}

int tty_ioctl(inode_t* ino, int cmd, void* buf) {
	if(!ino) {
		errno = EINVAL;
		return -1;
	}

	tty_t* tty = ino->userdata;
	if(!tty) {
		errno = ENOTTY;
		return -1;
	}


	spinlock_lock(&tty->lock);

	errno = 0;

	switch(cmd) {
		case TCGETS:
			memcpy(buf, &tty->ios, sizeof(struct termios));
			break;
		case TCSETS:
		case TCSETSW:
		case TCSETSF:
			memcpy(&tty->ios, buf, sizeof(struct termios));
			break;
		case TCGETA:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			errno = ENOSYS;
			break;
		case TIOCGLCKTRMIOS:
		case TIOCSLCKTRMIOS:
			if(!im_superuser())
				errno = EPERM;
			else
				errno = ENOSYS;
			break;
		case TIOCGWINSZ:
			memcpy(buf, &tty->win, sizeof(struct winsize));
			break;
		case TIOCSWINSZ:
			memcpy(&tty->win, buf, sizeof(struct winsize));
			break;
		case TCSBRK:
		case TCSBRKP:
			break;
		case TIOCSBRK:
		case TIOCCBRK:
			break;
		case TCXONC:
			errno = ENOSYS;
			break;
		case FIONREAD:
			*(int*) buf = tty->isize;
			break;
		case TIOCOUTQ:
			*(int*) buf = tty->osize;
			break;
		case TCFLSH:
			switch(*(int*) buf) {
				case TCIFLUSH:
					tty->isize = 0;
					break;
				case TCOFLUSH:
					tty->osize = 0;
					break;
				case TCIOFLUSH:
					tty->isize = tty->osize = 0;
					break;
				default:
					errno = EINVAL;
			}
			break;
		case TIOCSTI:
			while(*(char*) buf)
				tty->ibuffer[tty->isize++] = *(char*) buf++;
			break;
		case TIOCCONS:
		case TIOCSCTTY:
			if(!im_superuser())
				errno = EPERM;
			else
				errno = ENOSYS;
			break;
		case TIOCGPGRP:
			*(pid_t*) buf = tty->fg;
			break;
		case TIOCSPGRP:
			tty->fg = *(pid_t*) buf;
			break;
		case TIOCGSID:
			*(pid_t*) buf = tty->id;
			break;
		case TIOCEXCL:
			tty->exclmode = 1;
			break;
		case TIOCNXCL:
			tty->exclmode = 0;
			break;
		case TIOCGETD:
		case TIOCSETD:
			errno = ENOSYS;
			break;
		default:
#ifdef ENOIOCTLCMD
			errno = ENOIOCTLCMD;
#else
			errno = ENOSYS;
#endif
			break;
	}

	spinlock_unlock(&tty->lock);

	if(errno == 0)
		return 0;
	
	return -1;
}


int tty_write(inode_t* ino, char* ptr, int size) {
	if(!ino)
		return 0;
	
	if(!ptr)
		return 0;

	if(!size)
		return 0;

	tty_t* tty = ino->userdata;
	if(!tty) {
		errno = ENOTTY;
		return -1;
	}

	spinlock_lock(&tty->lock);
	memcpy(tty->obuffer, ptr, size);

	if(tty->output)
		tty->output(tty, ptr, size);

	spinlock_unlock(&tty->lock);

	return size;
}


int tty_read(inode_t* ino, char* ptr, int size) {
	if(!ino)
		return 0;

	if(!ptr)
		return 0;
	
	if(!size)
		return 0;

	tty_t* tty = ino->userdata;
	if(!tty) {
		errno = ENOTTY;
		return -1;
	}

	spinlock_lock(&tty->lock);
	spinlock_waiton(size > tty->isize);

	memcpy(ptr, tty->ibuffer, tty->isize);
	tty->isize -= size;

	if(tty->output)
		tty->output(tty, ptr, size);

	spinlock_unlock(&tty->lock);

	return size;
}


void tty_flush(inode_t* ino) {
	if(!ino)
		return;

	if(!ino->userdata)
		return;

	list_remove(ttydevs, (listval_t) ino->userdata);
	kfree(ino->userdata);
}


int tty_open(int* master, int* slave, char* name, struct termios* ios, struct winsize* win) {
	if(ttydevs == NULL) {
		list_init(ttydevs);
	}

	if(!master || !slave) {
		errno = EINVAL;
		return -1;
	}

	char fname[32];
	memset(fname, 0, 32);

	ksprintf(fname, "/dev/tty%d", (int) list_size(ttydevs));

	int fd = sys_open(fname, O_CREAT | O_EXCL | O_RDWR, S_IFCHR);
	if(fd < 0) {
		errno = ENOENT;
		return -1;
	}


	inode_t* ino = current_task->fd[fd];

	tty_t* tty = kmalloc(sizeof(tty_t));
	memset(tty, 0, sizeof(tty_t));


	ino->read = tty_read;
	ino->write = tty_write;
	ino->ioctl = tty_ioctl;
	ino->flush = tty_flush;

	ino->userdata = tty;

	tty->ios.c_iflag = TTYDEF_IOS_IFLAG;
	tty->ios.c_oflag = TTYDEF_IOS_OFLAG;
	tty->ios.c_cflag = TTYDEF_IOS_CFLAG;
	tty->ios.c_lflag = TTYDEF_IOS_LFLAG;
	
	memcpy(tty->ios.c_cc, ttydefchars, sizeof(cc_t) * NCCS);

	tty->win.ws_row = TTYDEF_WIN_ROWS;
	tty->win.ws_col = TTYDEF_WIN_COLS;
	tty->win.ws_xpixel = 0; /* unused */
	tty->win.ws_ypixel = 0; /* unused */

	tty->lock = 0;
	tty->isize = 0;
	tty->osize = 0;
	tty->exclmode = 0;

	tty->fg = current_task->pid;
	tty->bg = -1;

	tty->id = list_size(ttydevs);

	tty->output = __default_tty_output;

	list_add(ttydevs, (listval_t) tty);

	if(name)
		strcpy(name, fname);

	if(ios)
		memcpy(ios, &tty->ios, sizeof(struct termios));

	if(win)
		memcpy(win, &tty->win, sizeof(struct winsize)); 
		
	*master = fd;
	*slave = fd;

	return 0;
}

int tty_init() {
	int master, slave;
	char name[32];

	if(tty_open(&master, &slave, name, NULL, NULL) != 0) {
		kprintf("tty_open: failed with %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	kprintf("tty: created device at %s\n", name);

	sys_symlink(name, "/dev/stdin");
	sys_symlink(name, "/dev/stdout");
	sys_symlink(name, "/dev/stderr");


	inode_t* ino = current_task->fd[master];

	sys_close(master);
	sys_close(slave);

	
	kernel_task->fd[STDIN_FILENO] = ino;
	kernel_task->fd[STDOUT_FILENO] = ino;
	kernel_task->fd[STDERR_FILENO] = ino;

	return 0;	
}

