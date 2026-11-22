/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


long attr_access(inode_t* inode, int mask) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(current_task);

    if (current_task->fsuid == 0) {
        return 0;
    }


    struct stat st = {0};

    if (vfs_getattr(inode, &st) < 0) {
        return -EACCES;
    }


    mode_t have = st.st_mode & S_IRWXO;

    if (st.st_uid == current_task->fsuid) {
        have = (st.st_mode & S_IRWXU) >> 6;
    } else if (task_in_group(current_task, st.st_gid)) {
        have = (st.st_mode & S_IRWXG) >> 3;
    }

    mode_t want = 0;

    if (mask & R_OK) {
        want |= S_IROTH;
    }

    if (mask & W_OK) {
        want |= S_IWOTH;
    }

    if (mask & X_OK) {
        want |= S_IXOTH;
    }

    if ((have & want) != want) {
        return -EACCES;
    }

    return 0;
}


long attr_search(inode_t* dir) {

    DEBUG_ASSERT(dir);

    if (!dir->ops.getattr) {
        return 0;
    }

    return attr_access(dir, X_OK);
}


long attr_may_delete(inode_t* dir, uid_t owner) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(current_task);

    long e = attr_access(dir, W_OK | X_OK);

    if (e < 0) {
        return e;
    }

    if (current_task->fsuid == 0 || current_task->fsuid == owner) {
        return 0;
    }


    struct stat st = {0};

    if (vfs_getattr(dir, &st) < 0) {
        return -EACCES;
    }

    if ((st.st_mode & S_ISVTX) && st.st_uid != current_task->fsuid) {
        return -EPERM;
    }

    return 0;
}


long attr_chmod(inode_t* inode, mode_t mode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(current_task);


    struct stat st = {0};

    if (vfs_getattr(inode, &st) < 0)
        return -errno;


    bool privileged = current_task->fsuid == 0;

    if (unlikely(!privileged && st.st_uid != current_task->fsuid))
        return -EPERM;

    if (!privileged && !task_in_group(current_task, st.st_gid))
        mode &= ~S_ISGID;


    st.st_mode  = (st.st_mode & ~07777) | (mode & 07777);
    st.st_ctime = arch_timer_gettime();

    if (vfs_setattr(inode, &st) < 0)
        return -errno;

    return 0;
}


long attr_chown(inode_t* inode, uid_t uid, gid_t gid) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(current_task);


    struct stat st = {0};

    if (vfs_getattr(inode, &st) < 0)
        return -errno;


    bool privileged = current_task->fsuid == 0;
    bool owner      = current_task->fsuid == st.st_uid;

    if (uid != (uid_t)-1) {

        if (unlikely(!privileged && !(owner && uid == st.st_uid)))
            return -EPERM;

        st.st_uid = uid;
    }

    if (gid != (gid_t)-1) {

        if (unlikely(!privileged && !(owner && (gid == st.st_gid || task_in_group(current_task, gid)))))
            return -EPERM;

        st.st_gid = gid;
    }


    if (!S_ISDIR(st.st_mode) && (uid != (uid_t)-1 || gid != (gid_t)-1)) {

        st.st_mode &= ~S_ISUID;

        if (st.st_mode & S_IXGRP)
            st.st_mode &= ~S_ISGID;
    }

    st.st_ctime = arch_timer_gettime();

    if (vfs_setattr(inode, &st) < 0)
        return -errno;

    return 0;
}
