/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_SYSCALL_H
#define _APLUS_SYSCALL_H

#include <aplus.h>
#include <aplus/debug.h>

#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



#define SYSCALL(x, y, z...)           \
    z __section(".syscalls") struct { \
            uint32_t a;               \
            void* b;                  \
            char* name;               \
    } __packed __sc_##y = {(int)x, (void*)sys_##y, (char*)#y}


__BEGIN_DECLS

void syscall_init(void);
long syscall_invoke(unsigned long, long, long, long, long, long, long);
long syscall_restart();



/* Syscalls */

extern long sys_read(unsigned int fd, void* buf, size_t count);

extern long sys_write(unsigned int fd, const void* buf, size_t count);

extern long sys_open(const char* filename, int flags, mode_t mode);

extern long sys_close(unsigned int fd);

extern long sys_newstat(const char* filename, struct stat* statbuf);

extern long sys_newfstat(unsigned int fd, struct stat* statbuf);

extern long sys_newlstat(const char* filename, struct stat* statbuf);

extern long sys_lseek(unsigned int fd, off_t offset, unsigned int whence);

extern long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);

extern long sys_access(const char* filename, int mode);

extern long sys_exit(int error_code);

extern long sys_fsync(unsigned int fd);

extern long sys_getdents(unsigned int fd, struct dirent* dirent, unsigned int count);

extern long sys_chdir(const char* filename);

extern long sys_mkdir(const char* pathname, mode_t mode);

extern long sys_rmdir(const char* pathname);

extern long sys_creat(const char* pathname, mode_t mode);

extern long sys_link(const char* oldname, const char* newname);

extern long sys_unlink(const char* pathname);

extern long sys_symlink(const char* oldf, const char* newf);

extern long sys_readlink(const char* path, char* buf, int bufsiz);

extern long sys_chmod(const char* filename, mode_t mode);

extern long sys_chown(const char* filename, uid_t user, gid_t group);

// extern long sys_times (struct tms  * tbuf);

extern long sys_mknod(const char* filename, mode_t mode, unsigned dev);

extern long sys_mount(char const* dev_name, char const* dir_name, char const* type, unsigned long flags, void* data);

extern long sys_umount(char* name, int flags);

// extern long sys_clock_gettime (clockid_t which_clock, struct timespec  * tp);

// extern long sys_clock_nanosleep (clockid_t which_clock, int flags, const struct timespec  * rqtp, struct timespec  * rmtp);

extern long sys_fork(void);

extern long sys_vfork(void);

extern long sys_execve(const char* filename, const char** argv, const char** envp);

extern long sys_chroot(const char* pathname);

extern long sys_brk(unsigned long new_brk);

extern long sys_kill(pid_t pid, int sig);

extern long sys_rt_tgsigqueueinfo(pid_t tgid, pid_t tid, int sig, siginfo_t* uinfo);

__END_DECLS

#endif
