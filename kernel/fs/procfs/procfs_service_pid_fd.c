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



#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include "procfs.h"


/**
 * @brief Packs the pid and the descriptor a /proc/<pid>/fd/<n> symlink stands for into its service argument.
 */
#define PROCFS_PID_FD_ARG(pid, fd) ((void*)((((uintptr_t)(uint32_t)(pid)) << 32) | ((uintptr_t)(uint32_t)(fd))))
#define PROCFS_PID_FD_ARG_PID(arg) ((pid_t)(((uintptr_t)(arg)) >> 32))
#define PROCFS_PID_FD_ARG_FD(arg)  ((int)(((uintptr_t)(arg)) & 0xFFFFFFFFUL))


/**
 * @brief The state hanging off a /proc/<pid>/fd directory inode, holding the symlinks made on first lookup.
 */
typedef struct procfs_pid_fd_dir {

    pid_t pid;
    inode_t* children[CONFIG_OPEN_MAX];

} procfs_pid_fd_dir_t;


/**
 * @brief Parses a whole entry name as a descriptor number.
 *
 * @param name The name to parse.
 * @return The descriptor, or -1 if the name is not a descriptor this task could hold.
 */
static int __as_fd(const char* name) {

    if (unlikely(*name == '\0'))
        return -1;


    unsigned long v = 0;

    for (const char* p = name; *p; p++) {

        if (!isdigit(*p))
            return -1;

        v = (v * 10) + (unsigned long)(*p - '0');

        if (v >= CONFIG_OPEN_MAX)
            return -1;
    }

    return (int)v;
}


/**
 * @brief Generates the target of a /proc/<pid>/fd/<n> symlink, the path its descriptor is open on.
 *
 * @param inode The symlink inode.
 * @param buf Receives the link target.
 * @param size Receives the length of the target.
 * @param arg The packed pid and descriptor.
 * @return 0 on success, -1 if the descriptor is no longer open.
 */
static int procfs_pid_fd_fetch_link(inode_t* inode, char** buf, size_t* size, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    ssize_t e = procfs_task_fd_path(PROCFS_PID_FD_ARG_PID(arg), PROCFS_PID_FD_ARG_FD(arg), b.data, b.capacity);

    if (unlikely(e < 0))
        return -1;

    b.length = (size_t)e;

    *buf  = b.data;
    *size = b.length;

    return 0;
}


static inode_t* procfs_service_pid_fd_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(name);


    procfs_service_t* service = inode->userdata;
    procfs_pid_fd_dir_t* dir  = service->arg;

    DEBUG_ASSERT(dir);


    int fd = __as_fd(name);

    if (fd < 0)
        return errno = ENOENT, NULL;

    if (!procfs_task_fd_exists(dir->pid, fd))
        return errno = ENOENT, NULL;


    if (dir->children[fd] == NULL) {

        dir->children[fd] = procfs_service_inode(inode, name, S_IFLNK | 0777, procfs_pid_fd_fetch_link, PROCFS_PID_FD_ARG(dir->pid, fd));

        dir->children[fd]->ino = PROCFS_INO_PID_FD(dir->pid, fd);
    }

    return dir->children[fd];
}


static ssize_t procfs_service_pid_fd_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(e);

    if (unlikely(count == 0))
        return 0;

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;


    procfs_service_t* service = inode->userdata;
    procfs_pid_fd_dir_t* dir  = service->arg;

    DEBUG_ASSERT(dir);


    size_t i = 0;

#define __emit(_ino, _type, _name)                          \
    do {                                                    \
                                                            \
        if (pos-- > 0)                                      \
            break;                                          \
                                                            \
        e[i].d_ino  = (_ino);                               \
        e[i].d_off  = (off_t)(i);                           \
        e[i].d_type = (_type);                              \
                                                            \
        strncpy(e[i].d_name, (_name), sizeof(e[i].d_name)); \
        e[i].d_name[sizeof(e[i].d_name) - 1] = '\0';        \
                                                            \
        e[i].d_reclen = sizeof(struct dirent);              \
                                                            \
        if (++i == count)                                   \
            return (ssize_t)i;                              \
                                                            \
    } while (0)


    __emit(PROCFS_INO_PID_FILE(dir->pid, PROCFS_PID_SLOT_FD), DT_DIR, ".");
    __emit(PROCFS_INO_PID(dir->pid), DT_DIR, "..");

#undef __emit


    if (pos < 0)
        return (ssize_t)i;


    int* open = (int*)kcalloc(CONFIG_OPEN_MAX, sizeof(int), GFP_KERNEL);

    if (unlikely(!open))
        return (ssize_t)i;

    size_t n = procfs_task_fd_list(dir->pid, open, CONFIG_OPEN_MAX);


    for (size_t j = 0; j < n; j++) {

        if (pos-- > 0)
            continue;

        e[i].d_ino  = PROCFS_INO_PID_FD(dir->pid, open[j]);
        e[i].d_off  = (off_t)(i);
        e[i].d_type = DT_LNK;

        snprintf(e[i].d_name, sizeof(e[i].d_name), "%d", open[j]);

        e[i].d_reclen = sizeof(struct dirent);

        if (++i == count)
            break;
    }

    kfree(open);

    return (ssize_t)i;
}


inode_t* procfs_service_pid_fd_inode(inode_t* parent, pid_t pid) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_PROCFS);


    procfs_pid_fd_dir_t* dir = kcalloc(1, sizeof(procfs_pid_fd_dir_t), GFP_KERNEL);

    dir->pid = pid;


    inode_t* inode = procfs_service_inode(parent, "fd", S_IFDIR | 0555, NULL, dir);

    inode->ino         = PROCFS_INO_PID_FILE(pid, PROCFS_PID_SLOT_FD);
    inode->ops.finddir = procfs_service_pid_fd_finddir;
    inode->ops.readdir = procfs_service_pid_fd_readdir;

    return inode;
}


/**
 * @brief Releases a /proc/<pid>/fd directory and every symlink it handed out.
 *
 * @param inode The directory inode, or NULL if the directory was never looked up.
 */
void procfs_service_pid_fd_free(inode_t* inode) {

    if (inode == NULL)
        return;


    procfs_service_t* service = inode->userdata;

    if (likely(service)) {

        procfs_pid_fd_dir_t* dir = service->arg;

        if (likely(dir)) {

            for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

                if (dir->children[i] == NULL)
                    continue;

                kfree(dir->children[i]->userdata);
                kfree(dir->children[i]);
            }

            kfree(dir);
        }

        kfree(service);
    }

    kfree(inode);
}
