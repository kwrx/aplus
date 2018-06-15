/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_H
#define _APLUS_H

#include <aplus/base.h>



#ifndef __ASSEMBLY__
#include <libc.h>


typedef struct bootargs {
    struct {
        uint64_t size;
        uint32_t pagesize;
        uintptr_t start;
    } memory;

    struct {
        struct {
            uintptr_t ptr;
            size_t size;
            uintptr_t cmdline;
            uintptr_t reserved;
        } __packed *ptr;
        size_t count;
    } modules;

    struct {
        uint16_t width;
        uint16_t height;
        uint16_t depth;
        uint32_t pitch;
        uintptr_t base;
        uint32_t size;
    } lfb;

    struct {
        char* args;
        int length;
    } cmdline;

    struct {
        uint32_t num;
        uintptr_t addr;
        uint32_t size;
        uint32_t shndx;
    } exec;

    struct {
        uint32_t speed;
        uint32_t cores;
        uint32_t threads;
        char* family;
    } cpu;

    int flags;
} bootargs_t;



#define EXPORT(f)               \
    __section(".exports")       \
    struct {                    \
        char* name;             \
        void* addr;             \
    } __export_##f = {          \
        (char*) #f,             \
        (void*) &f              \
    };



extern bootargs_t* mbd;


extern int sys_mount(const char* dev, const char* dir, const char* fstype, unsigned long int options, const void* data);
extern int sys_chown(const char*, uid_t, gid_t);
extern int sys_chroot(const char*);
extern int sys_clone(int (*)(void*), void*, int, void*);
extern int sys_close(int);
extern void sys_exit(int);
extern int sys_fork(void);
extern int sys_fstat(int, struct stat*);
extern int sys_getdents(int, struct dirent*, size_t);
extern pid_t sys_getpid(void);
extern int isatty(int);
extern int sys_kill(pid_t, int);
extern int sys_link(const char*, const char*);
extern off_t sys_lseek(int, off_t, int);
extern int sys_open(const char*, int, mode_t);
extern int sys_read(int, void*, size_t);
extern void* sys_sbrk(ptrdiff_t);
extern clock_t sys_times(struct tms*);
extern int sys_wait(int*);
extern int sys_waitpid(pid_t, int*, int);
extern int sys_write(int, void*, size_t);
extern void sys_yield(void);
extern void* sys_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);
extern int sys_munmap(void* addr, size_t len);
extern int sys_execve(const char* filename, char* const argv[], char* const envp[]);
extern int sys_mkfifo(const char* pathname, mode_t mode);
extern int sys_symlink(const char*, const char*);
extern int sys_fcntl(int, int, long);
extern int sys_ioctl(int, int, void*);
extern int sys_unlink(const char*);
extern int sys_clock_gettime(clockid_t id, struct timespec *tv);
extern int sys_pause(void);
extern void sys_sync(void);
extern int sys_poll(struct pollfd*, nfds_t, int);
extern int sys_stat(const char*, struct stat*);
extern int sys_mknod(const char*, mode_t, dev_t);
extern pid_t sys_getpgid(pid_t);
extern int sys_fstatvfs(int, struct statvfs*);
extern int sys__llseek(int, unsigned int, unsigned int, off64_t*, int);
extern int sys_fsync(int);
extern int sys_rt_sigprocmask(int, const sigset_t*, sigset_t*, size_t);
extern int sys_setitimer(int, const struct itimerval*, struct itimerval*);
extern int sys_getitimer(int, struct itimerval*);
extern int sys_setpriority(int, id_t, int);
extern int sys_wait4(pid_t, int*, int, struct rusage*);
extern int sys_fcntl64(int fd, int cmd, long arg);

extern int mounts_init();
extern int core_init();
extern int libk_init();

/* See src/init/hostname.c */
extern char* hostname;

#endif
#endif
