/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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


#ifndef _APLUS_SYSCALL_H
#define _APLUS_SYSCALL_H

#include <aplus.h>
#include <aplus/debug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>



#define SYSCALL(x, y, z...)                     \
    z                                           \
    __attribute__((section(".syscalls")))       \
    struct {                                    \
        uint32_t a;                             \
        void* b;                                \
        char* name;                             \
    } __packed __sc_##y = {                     \
        (int) x,                                \
        (void*) sys_##y,                        \
        (char*) #y                              \
    } 


void syscall_init(void);
long syscall_invoke(long, long, long, long, long, long, long);


/* Syscalls */

extern long sys_read (unsigned int fd, void __user * buf, size_t count);

extern long sys_write (unsigned int fd, const void __user * buf, size_t count);

extern long sys_open (const char __user * filename, int flags, mode_t mode);

extern long sys_close (unsigned int fd);

extern long sys_newstat (const char __user * filename, struct stat __user * statbuf);

extern long sys_newfstat (unsigned int fd, struct stat __user * statbuf);

extern long sys_newlstat (const char __user * filename, struct stat __user * statbuf);

extern long sys_lseek (unsigned int fd, off_t offset, unsigned int whence);

extern long sys_ioctl (unsigned int fd, unsigned int cmd, unsigned long arg);

extern long sys_access (const char __user * filename, int mode);

extern long sys_exit (int error_code);

extern long sys_fsync (unsigned int fd);

extern long sys_getdents (unsigned int fd, struct dirent __user * dirent, unsigned int count);

extern long sys_chdir (const char __user * filename);

extern long sys_mkdir (const char __user * pathname, mode_t mode);

extern long sys_rmdir (const char __user * pathname);

extern long sys_creat (const char __user * pathname, mode_t mode);

extern long sys_link (const char __user * oldname, const char __user * newname);

extern long sys_unlink (const char __user * pathname);

extern long sys_symlink (const char __user * old, const char __user * new);

extern long sys_readlink (const char __user * path, char __user * buf, int bufsiz);

extern long sys_chmod (const char __user * filename, mode_t mode);

extern long sys_chown (const char __user * filename, uid_t user, gid_t group);

//extern long sys_times (struct tms __user * tbuf);

extern long sys_mknod (const char __user * filename, mode_t mode, unsigned dev);

extern long sys_mount (char __user * dev_name, char __user * dir_name, char __user * type, unsigned long flags, void __user * data);

extern long sys_umount (char __user * name, int flags);

//extern long sys_clock_gettime (clockid_t which_clock, struct timespec __user * tp);

//extern long sys_clock_nanosleep (clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);

extern long sys_clone (int (*fn)(void*), void __user * stack, unsigned long flags, void* arg);

extern long sys_fork (void);

extern long sys_vfork (void);

extern long sys_execve (const char __user * filename, const char __user ** argv, const char __user ** envp);

extern long sys_chroot(const char __user * pathname);

extern long sys_brk (unsigned long new_brk);


#endif