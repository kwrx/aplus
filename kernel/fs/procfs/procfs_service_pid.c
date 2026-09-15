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

#include <aplus/utils/cache.h>

#include "procfs.h"


/* The contents of a /proc/<pid> directory, declared once. readdir() and finddir() are both
   driven from this table: they used to be a hardcoded macro list and a strcmp() chain kept
   in step by hand, which is how readdir came to advertise a "cmdline" that finddir could
   not resolve. */
typedef struct procfs_pid_entry {

    const char* name;
    mode_t mode;
    uint8_t slot;

    int (*fetch)(inode_t*, char**, size_t*, void*);

} procfs_pid_entry_t;


static const procfs_pid_entry_t procfs_pid_table[] = {
    {"stat",    S_IFREG | 0444, 1, procfs_pid_fetch_stat   },
    {"status",  S_IFREG | 0444, 2, procfs_pid_fetch_status },
    {"statm",   S_IFREG | 0444, 3, procfs_pid_fetch_statm  },
    {"cmdline", S_IFREG | 0444, 4, procfs_pid_fetch_cmdline},
    {"io",      S_IFREG | 0444, 5, procfs_pid_fetch_io     },
};

#define PROCFS_PID_ENTRIES (sizeof(procfs_pid_table) / sizeof(procfs_pid_table[0]))


/* Hangs off the directory inode. The child inodes are created on first lookup and kept,
   because this VFS has no way to release an inode once finddir() has handed it out. */
typedef struct procfs_pid_dir {

    pid_t pid;
    inode_t* children[PROCFS_PID_ENTRIES];

} procfs_pid_dir_t;


static cache_t cache = {0};


static inode_t* procfs_service_pid_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(name);


    procfs_service_t* service = inode->userdata;
    procfs_pid_dir_t* dir     = service->arg;

    DEBUG_ASSERT(dir);


    /* Dispatch on the name being looked up. This function used to branch on inode->name --
       the directory's own name -- and hand back the directory itself for every lookup, so
       /proc/<pid>/cmdline resolved to /proc/<pid> and nothing under a pid was reachable. */
    for (size_t i = 0; i < PROCFS_PID_ENTRIES; i++) {

        if (strcmp(name, procfs_pid_table[i].name) != 0)
            continue;


        //? No lock here: vfs_finddir() already holds inode->lock across this call, which is
        //? the mutual exclusion the lazily-created child needs. Taking it again deadlocks.
        if (dir->children[i] == NULL) {

            dir->children[i] = procfs_service_inode(inode, procfs_pid_table[i].name, procfs_pid_table[i].mode, procfs_pid_table[i].fetch, (void*)((uintptr_t)dir->pid));

            dir->children[i]->ino = PROCFS_INO_PID_FILE(dir->pid, procfs_pid_table[i].slot);
        }

        return dir->children[i];
    }

    return errno = ENOENT, NULL;
}


static ssize_t procfs_service_pid_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

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
    procfs_pid_dir_t* dir     = service->arg;

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


    __emit(PROCFS_INO_PID(dir->pid), DT_DIR, ".");
    __emit(PROCFS_INO_ROOT, DT_DIR, "..");

    for (size_t j = 0; j < PROCFS_PID_ENTRIES; j++) {
        __emit(PROCFS_INO_PID_FILE(dir->pid, procfs_pid_table[j].slot), DT_REG, procfs_pid_table[j].name);
    }

#undef __emit

    return (ssize_t)i;
}


/**
 * @brief Build the /proc/<pid> directory inode for a pid.
 *
 * Only ever called for a pid that procfs_pid_exists() has already confirmed -- cache_get()
 * ends in a PANIC_ASSERT() on a NULL value, so a miss handler that rejected a bad pid would
 * halt the kernel rather than return ENOENT.
 */
static inode_t* procfs_service_pid_cache_fetch(cache_t* c, inode_t* parent, cache_key_t key) {

    DEBUG_ASSERT(c);
    DEBUG_ASSERT(parent);


    pid_t pid = (pid_t)((uintptr_t)key);

    char name[32] = {0};
    snprintf(name, sizeof(name), "%d", pid);


    procfs_pid_dir_t* dir = kcalloc(1, sizeof(procfs_pid_dir_t), GFP_KERNEL);

    dir->pid = pid;


    inode_t* inode = procfs_service_inode(parent, name, S_IFDIR | 0555, NULL, dir);

    inode->ino         = PROCFS_INO_PID(pid);
    inode->ops.finddir = procfs_service_pid_finddir;
    inode->ops.readdir = procfs_service_pid_readdir;

    return inode;
}

static void procfs_service_pid_cache_commit(cache_t* c, inode_t* parent, cache_key_t key, inode_t* value) {

    (void)c;
    (void)parent;
    (void)key;
    (void)value;
}

static void procfs_service_pid_cache_release(cache_t* c, inode_t* parent, cache_key_t key, inode_t* value) {

    (void)c;
    (void)parent;
    (void)key;

    DEBUG_ASSERT(value);


    procfs_service_t* service = value->userdata;

    if (likely(service)) {

        procfs_pid_dir_t* dir = service->arg;

        if (likely(dir)) {

            for (size_t i = 0; i < PROCFS_PID_ENTRIES; i++) {

                if (dir->children[i] == NULL)
                    continue;

                //? Each child owns a procfs_service_t of its own; freeing only the inode
                //? leaked one per file.
                kfree(dir->children[i]->userdata);
                kfree(dir->children[i]);
            }

            kfree(dir);
        }

        kfree(service);
    }

    kfree(value);
}


inode_t* procfs_service_pid_inode(inode_t* parent, pid_t pid) {

    DEBUG_ASSERT(parent);
    DEBUG_ASSERT(parent->sb);
    DEBUG_ASSERT(parent->sb->fsid == FSID_PROCFS);

    if (unlikely(pid <= 0))
        return errno = ENOENT, NULL;


    /* Checked before the cache is consulted, not inside the miss handler: an unchecked pid
       meant stat("/proc/999999") succeeded and allocated an inode that was never evicted,
       so a loop of them exhausted kernel memory from an unprivileged shell. */
    if (!procfs_pid_exists(pid))
        return errno = ENOENT, NULL;


    return cache_get(&cache, pid);
}


void procfs_service_pid_init(inode_t* parent) {

    cache_ops_t ops = {
        .fetch   = (cache_fetch_handler_t)procfs_service_pid_cache_fetch,
        .commit  = (cache_commit_handler_t)procfs_service_pid_cache_commit,
        .release = (cache_release_handler_t)procfs_service_pid_cache_release,
    };

    cache_init(&cache, &ops, SIZE_MAX, parent);
}
