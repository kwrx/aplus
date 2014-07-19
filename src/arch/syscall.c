//
//  syscall.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdint.h>
#include <string.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/times.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


#include <aplus.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>

#include <setjmp.h>

#define SYSCALL_MAGIC		0x0BADC0DE

/* Set by linker */
extern uint32_t syscall_handlers_start;
extern uint32_t syscall_handlers_end;

extern task_t* current_task;
extern task_t* kernel_task;

extern inode_t* fs_root;

typedef struct syscall_handler_func {
	uint32_t magic;
	uint32_t number;
	uint32_t (*handler)(uint32_t par0, uint32_t par1, uint32_t par2, uint32_t par3, uint32_t par4);
} __attribute__((packed)) syscall_handler_t;



#define syscall(func, n)																				\
	uint32_t syscall_##func(uint32_t par0, uint32_t par1, uint32_t par2, uint32_t par3, uint32_t par4);	\
	syscall_handler_t syscall_handler_##func __attribute__((section(".syscall_handlers"))) = {			\
		SYSCALL_MAGIC, (uint32_t)n,  syscall_##func														\
	};																									\
	uint32_t syscall_##func(uint32_t par0, uint32_t par1, uint32_t par2, uint32_t par3, uint32_t par4)

	

	
	
static inode_t* open_inode(char* name, int flags, int mode) {
	inode_t* index = current_task->cwd;
	
	if(name[0] == '/') {
		index = fs_root;
		name++;
	}
	
	if(name[0] == 0) {
		if(!index)
			return -1;
		
		return index;
	}
	
	char* s = strdup(name);
	char* p = s;
	
	while(*s) {
		if(p = strchr(s, '/')) {
			*p++ = 0;
			
			index = fs_finddir(index, s);
			if(!index) {
				errno = ENOENT;
				return -1;
			}
			
			if(S_ISLNK(index->mask))
				if(index->link)
					index = index->link;
			
			s = p;
		} else {
			
			inode_t* ent = fs_finddir(index, s);
			if(!ent) {
				if(flags & O_CREAT) {
					ent = fs_creat(index, s, mode);
				} else {
					errno = ENOENT;
					return -1;
				}
			} else {
				if(flags & O_CREAT && flags & O_EXCL) {
					errno = EEXIST;
					return -1;
				}
			}

			if(!(flags & O_NOFOLLOW))
				if(S_ISLNK(ent->mask))
					if(ent->link)
						ent = ent->link;
						
				
						
			if(flags & O_DIRECTORY) {
				if(!(S_ISDIR(ent->mask & S_IFDIR))) {
					errno = ENOTDIR;
					return -1;
				}
			}

			//if(spinlock_trylock(&ent->flock) != 0) {
				//errno = EACCES;
				//return -1;
			//}
			
			if(flags & O_TRUNC)
				fs_trunc(ent);
			
			
			return ent;
		}
	}
	
	return -1;
}	
	

syscall(_exit, 0) {
	if(!current_task)
		return -1;
					
	return task_exit(par0);
}

syscall(_chown, 1) {
	if(!current_task)
		return -1;
	
	char* name = (char*) par0;
	inode_t* f = open_inode(name, O_RDONLY, 0644);
	if(f == -1)
		return -1;
		
	f->uid = par1;
	f->gid = par2;
	
	return 0;
}

syscall(_close, 2) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	spinlock_unlock(&ino->flock);
	
	current_task->fd[par0] = 0;
	return 0;
}



syscall(_execve, 3) {

#define duplicate(dst, src)																\
	int dst##_index = 0;																\
	while(src[dst##_index]) {															\
		dst[dst##_index] = kmalloc(strlen(src[dst##_index]));							\
		memcpy(dst[dst##_index], src[dst##_index], strlen(src[dst##_index]));			\
		dst##_index += 1;																\
	} dst##_index = 0


	if(!current_task)
		return -1;

		
	inode_t* fd = open_inode(par0, O_RDONLY, 0644);
	if(fd == -1) {
		kprintf("execve: \"%s\" not found!\n", par0);

		errno = ENOENT;
		return -1;
	}
		
	
	sched_disable();
	
	uint32_t size = fd->length;
	size = (size & ~0xFFF) + 0x1000;
	
	uint32_t pmm = kmalloc(size);
	int br = 0;
	if((br = fs_read(fd, fd->length, pmm)) != fd->length) {
		kprintf("execve: could not read all executable\n");
		
		kfree(pmm);
		errno = EIO;
		return -1;
	}
	
	
	void* entry = elf_load_file(pmm, size);
	if(entry == NULL) {
	
		kprintf("execve: could not load elf executable\n");
	
		kfree(pmm);
		errno = ENOEXEC;
		return -1;
	}
	

	char** env = kmalloc(1024);
	char** argv = kmalloc(1024);
	
	duplicate(env, ((char**)par2));
	duplicate(argv, ((char**)par1));
	
	task_t* t = task_create(NULL, entry);
	t->image = pmm;
	t->imagelen = size;
	t->argv = argv;
	t->environ = env;
	t->exe = fd;
	
	for(int i = 0; i < size; i += 4096)
		vmm_map(t->vmm, i + pmm, i + VIRTUAL_CODE_ADDRESS);

	sched_enable();
	int retcode = task_wait(t);
	kfree(pmm);
	
	_exit(retcode);
}



static jmp_buf env;

static void child_eip() {
	longjmp(env, 1);
}

syscall(_fork, 4) {
	if(!current_task)
		return -1;
		
	int ret = setjmp(env);
	if(ret == 0) {
		sched_disable();
		task_t* tsk = task_create(NULL, child_eip);
		if(!tsk) {
			errno = EFAULT;
			return -1;
		}
		
		if(current_task->image && (current_task->imagelen > 0)) {
			tsk->imagelen = current_task->imagelen;
			tsk->image = kmalloc(tsk->imagelen);
			
			for(int i = 0; i < tsk->imagelen; i += 4096)
				vmm_map(tsk->vmm, i + (uint32_t) tsk->image, i + VIRTUAL_CODE_ADDRESS);
			
			memcpy(tsk->image, current_task->image, tsk->imagelen);
		}
		
		task_clonefd(tsk);
		tsk->cwd = current_task->cwd;
		tsk->argv = current_task->argv;
		tsk->environ = current_task->environ;
		tsk->exe = current_task->exe;
		tsk->priority = current_task->priority;
		tsk->state = current_task->state;
		tsk->signal_handler = current_task->signal_handler;
		
		
		
		
		uint32_t oldesp = env->esp;
		uint32_t oldebp = env->ebp;
			
		uint32_t stackbuf = (uint32_t) kmalloc(4096);	
			
		uint32_t newesp = (uint32_t) stackbuf | (oldesp & 0xFFF);
		uint32_t newebp = (uint32_t) stackbuf | (oldebp & 0xFFF);
		
		for(int i = 0; i < 4096; i++)
			((uint8_t*) stackbuf)[i] = ((uint8_t*) (oldesp & ~0xFFF))[i];
			
		env->esp = newesp;
		env->ebp = newebp;
		
		sched_enable();
		return tsk->pid;
	} else
		return 0;
}

syscall(_fstat, 5) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	
	struct stat* st = (struct stat*) par1;
	if(!st) {
		errno = EINVAL;
		return -1;
	}
	
	if(ino->dev) {
		if(ino->dev->mask & S_IFBLK || ino->dev->mask & S_IFCHR)
			st->st_rdev = ino->dev->inode;
			
		st->st_dev = ino->dev->inode;
	} else
		st->st_dev = 0;
		
	st->st_ino = ino->inode;
	st->st_mode = ino->mask;
	st->st_nlink = ino->links_count;
	st->st_uid = ino->uid;
	st->st_gid = ino->gid;
	st->st_size = ino->length;
	st->st_atime = ino->atime;
	st->st_mtime = ino->mtime;
	st->st_ctime = ino->ctime;
	st->st_blksize = 512;
	st->st_blocks = st->st_size / st->st_blksize;

	return 0;
}

syscall(_getpid, 6) {
	if(!current_task)
		return -1;
	
	return current_task->pid;
}

syscall(gettimeofday, 7) {
	struct timeval* p = (struct timeval*) par0;
	if(!p) {
		errno = EINVAL;
		return -1;
	}
	
	p->tv_sec = pit_gettime();
	p->tv_usec = pit_getticks() * 1000;
	
	return 0;
}

syscall(_kill, 8) {
	task_t* t = task_getbypid(par0);
	if(!t) {
		errno = ESRCH;
		return -1;
	}

	if(t == kernel_task) {
		errno = EACCES;
		return -1;
	}
	
	t->signal_sig = par1;
	return 0;
}

syscall(_link, 9) {
	return symlink(par0, par1);
}

syscall(_lseek, 10) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	if(par1 > ino->length) {
		errno = EINVAL;
		return -1;
	}
	
	switch(par2) {
		case SEEK_SET:
			ino->position = par1;
			break;
		case SEEK_END:
			ino->position = ino->length - par1;
			break;
		case SEEK_CUR:
			ino->position += par1;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	
	if(ino->position > ino->length)
		ino->position = ino->length;
		
	return ino->position;
}

syscall(_open, 11) {
	if(!current_task)
		return -1;
	
	char* name = (char*) par0;
	int flags = par1;
	int mode = par2;
	
	inode_t* f = open_inode(name, flags, mode);
	if(f == -1)
		return -1;
		
		
	uint32_t entry = 0;
	while(current_task->fd[entry]) {
		if(entry >= TASK_MAX_FD) {
			errno = EMFILE;
			return -1;
		}
		
		entry++;
	}

		
	current_task->fd[entry] = f;
	return entry;
}


syscall(_read, 12) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	if(ino->mask & S_IFLNK)
		if(ino->link)
			ino = ino->link;
	
	int c = fs_read(ino, par2, par1);
	ino->position += c;
	
	return c;
}


syscall(_readlink, 13) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	int c = strncpy((char*)par1, ino->name, par2);	
	return c;
}

syscall(_stat, 14) {
	char* name = par0;
	struct stat* st = par1;
	
	if(!name || !st) {
		errno = EINVAL;
		return -1;
	}
	
	int fd = syscall__open(name, O_RDONLY, 0644, 0, 0);
	if(fd == -1)
		return -1;
		
	if(syscall__fstat(fd, st, 0, 0, 0) == -1) {
		syscall__close(fd, 0, 0, 0, 0);
		return -1;
	}
		
	syscall__close(fd, 0, 0, 0, 0);
	return 0;
}

syscall(_symlink, 15) {
	char* s = par0;
	char* d = par1;
	
	if(!s || !d) {
		errno = EINVAL;
		return -1;
	}
	
	inode_t* is = open_inode(s, O_RDONLY, 0644);
	if(is == -1) {
		errno = ENOENT;
		return -1;
	}
	
	if(is->links_count > INODE_MAX_LINKS) {
		errno = EMLINK;
		return -1;
	}
	
	inode_t* id = open_inode(d, O_RDONLY | O_CREAT | O_EXCL, S_IFLNK);
	if(id == -1) {
		errno = ENOENT;
		return -1;
	}
	
	is->links_count += 1;
	id->link = is;
	
	return 0;
}

syscall(_times, 16) {
	if(!current_task)
		return -1;

	struct tms* p = par0;
	if(!p) {
		errno = EINVAL;
		return -1;
	}
	
	p->tms_utime = current_task->clock;
	p->tms_stime = 0;
	p->tms_cutime = 0;
	p->tms_cstime = 0;
	
	return 0;
}

syscall(_unlink, 17) {
	inode_t* lnk = open_inode(par0, O_RDONLY | O_NOFOLLOW, 0644);
	if(lnk == -1) {
		errno = ENOENT;
		return -1;
	}
	
	if(lnk->parent) {
		if(S_ISLNK(lnk->mask))
			if(lnk->link)
				if(lnk->link->links_count > 0)
					lnk->link->links_count -= 1;
					
		if(fs_remove(lnk->parent, lnk->name) != 0)
			return -1;
			
		return 0;
	}
	
	errno = EIO;
	return -1;
}

syscall(_wait, 18) {
	if(!current_task)
		return -1;
		
	task_t* child = task_child();
	if(!child) {
		errno = ECHILD;
		return -1;
	}
	

	int retcode = task_wait(child);
	if(par0)
		*(int*) par0 = retcode;
	
	return 0;
}

syscall(_write, 19) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	if(ino->mask & S_IFLNK)
		if(ino->link)
			ino = ino->link;
	
	int c = fs_write(ino, par2, par1);
	ino->position += c;
	
	return c;
}

syscall(_readdir, 20) {
	if(!current_task)
		return 0;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return 0;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return 0;
	}
	
	if(ino->mask & S_IFLNK)
		if(ino->link)
			ino = ino->link;
			
	if(!(ino->mask & S_IFDIR)) {
		errno = ENOTDIR;
		return 0;
	}
	
	ino->position = par1;
	return fs_readdir(ino);
}


syscall(ioctl, 21) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	if(ino->mask & S_IFLNK)
		if(ino->link)
			ino = ino->link;
			
	return fs_ioctl(ino, par1, par2);
}

syscall(mount, 22) {
	if(!current_task)
		return -1;
	
	inode_t* dev = 0;
	inode_t* dir = 0;
	
	if(strlen(par0) > 0) {
		int devfd = open((char*) par0, O_RDONLY, 0644);
		if(devfd < 0)
			return -1;
			
		dev = current_task->fd[devfd];
		close(devfd);
	}
	
	
	if(strlen(par1) > 0) {
		int dirfd = open((char*) par1, O_CREAT, S_IFDIR);
		if(dirfd < 0)
			return -1;
			
		dir = current_task->fd[dirfd];
		close(dirfd);
	}
	
	return fs_mount(dev, dir, par3, (char*) par2);
}

syscall(umount, 23) {
	if(!current_task)
		return -1;
		
	int devfd = open((char*) par0, O_RDONLY, 0644);
	if(devfd < 0)
		return -1;
		
	inode_t* dev = current_task->fd[devfd];
	close(devfd);
	
	return fs_umount(dev);
}

syscall(tell, 24) {
	if(!current_task)
		return -1;
		
	if(par0 > TASK_MAX_FD) {
		errno = EINVAL;
		return -1;
	}
		
	inode_t* ino = current_task->fd[par0];
	if(!ino) {
		errno = EBADF;
		return -1;
	}
	
	if(ino->mask & S_IFLNK)
		if(ino->link)
			ino = ino->link;
			
	return ino->position;
}

syscall(__get_argv, 25) {
	if(!current_task)
		return 0;
		
	return current_task->argv;
}

syscall(__get_env, 26) {
	if(!current_task)
		return 0;
		
	return current_task->environ;
}

syscall(pipe, 27) {
	if(!current_task)
		return 0;
		
	#define PIPE_SIZE		(1024 * 16)
	static uint32_t pipe_count = 0;
	
	char* b = kmalloc(32);
	sprintf(b, "/dev/pipe%u", pipe_count++);
		
	inode_t* p = pipe_create(b, PIPE_SIZE);
	if(!p) {
		kfree(b);
		
		errno = EINVAL;
		return -1;
	}
	
	int* fd = (int*) par0;
	
	uint32_t entry = 0;
	while(current_task->fd[entry]) {
		if(entry >= TASK_MAX_FD) {
			errno = EMFILE;
			return -1;
		}
		
		entry++;
	}
	
	fd[0] = entry;
	current_task->fd[entry] = p;
	
	entry = 0;
	while(current_task->fd[entry]) {
		if(entry >= TASK_MAX_FD) {
			errno = EMFILE;
			return -1;
		}
		
		entry++;
	}
	
	fd[1] = entry;
	current_task->fd[entry] = p;
	
	return 0;
}

syscall(chdir, 28) {
	if(!current_task)
		return -1;
		
	if(!par0)
		return -1;
		
	inode_t* d = open_inode(par0, O_RDONLY | O_DIRECTORY, 0);
	if(d == -1)
		return -1;
		
	current_task->cwd = (inode_t*) d;
	return 0;
}


syscall(__install_signal_handler, 29) {
	if(!current_task)
		return -1;
		
	if(!par0)
		return -1;
		
	current_task->signal_handler = par0;
	return 0;
}

syscall(malloc, 30) {
	return kmalloc(par0);
}

syscall(free, 31) {
	kfree(par0);
}

syscall(aplus_thread_create, 32) {
	task_t* t = task_create_with_data(current_task->vmm, par0, par1);
	t->priority = par2;

	task_clonefd(t);

	return t->pid;
}

syscall(__idle, 33) {
	task_idle();
}

syscall(__wakeup, 34) {
	task_wakeup();
}

syscall(__zombie, 35) {
	task_zombie();
}

syscall(aplus_device_create, 36) {
	if(!current_task)
		return 0;

	int fd = open(par0, O_CREAT | O_EXCL, par1);
	if(fd < 0)
		return 0;
	
	inode_t* rn = current_task->fd[fd];
	close(fd);

	return rn;
}

syscall(fcntl, 37) {
	errno = ENOSYS;
	return -1;
}

syscall(chroot, 38) {
	inode_t* ino = open_inode(par0, O_RDONLY | O_DIRECTORY, 0);
	if(!ino) {
		errno = ENOENT;
		return -1;
	}
	
	return fs_chroot(ino);
}


uint32_t syscall_handler(regs_t* r) {

	syscall_handler_t* handlers = (syscall_handler_t*) &syscall_handlers_start;
	
	while(handlers < &syscall_handlers_end) {
		if(handlers->magic != SYSCALL_MAGIC)
			break;
	
		if(handlers->number == r->eax)
			return handlers->handler(r->ebx, r->ecx, r->edx, r->esi, r->edi);
			
		handlers = &handlers[1];
	}
	
	kprintf("syscall: handler %d not found\n", r->eax);
	
	errno = ENOSYS;
	return -1;
}
