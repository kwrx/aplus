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

#include <poll.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include <aplus/utils/ptr.h>


/**
 * @brief The bit of inode->refcount set once the inode's directory entry is gone; the rest counts holders.
 */
#define INODE_REF_UNLINKED 0x80000000U


fstable_t __fs_table[VFS_MAX_FILESYSTEMS] = {0};
fstable_t* fs_table = &__fs_table[0];


void vfs_init(void) {

    int i = 0;
#include "fstable.c.in"


#if DEBUG_LEVEL_INFO

    for (i = 0; fs_table[i].id; i++)
        kprintf("vfs: found filesystem '%s'\n", fs_table[i].name);

#endif


    rootfs_init();
    fd_init();

#if DEBUG_LEVEL_INFO
    kprintf("vfs: ready!\n");
#endif
}


int vfs_mount(inode_t* dev, inode_t* dir, const char* fs, int flags, const char* args) {

    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(fs);


    int i;
    for (i = 0; fs_table[i].id; i++) {

        if (strcmp(fs_table[i].name, fs) != 0)
            continue;


        DEBUG_ASSERT(fs_table[i].mount);
        DEBUG_ASSERT(fs_table[i].umount);


        int e;
        if ((e = fs_table[i].mount(dev, dir, flags, args)) < 0)
            return e;

        DEBUG_ASSERT(dir->sb);


        struct inode_ops ops;
        memcpy(&ops, &dir->ops, sizeof(ops));
        memcpy(&dir->ops, &dir->sb->ops, sizeof(ops));
        memcpy(&dir->sb->ops, &ops, sizeof(ops));


        dir->ino ^= dir->sb->ino;
        dir->sb->ino ^= dir->ino;
        dir->ino ^= dir->sb->ino;


        vfs_inode_get(dir);

        if (dev)
            vfs_inode_get(dev);


#if DEBUG_LEVEL_INFO
        kprintf("mount: volume %s mounted on %s with %s\n", dev ? dev->name : "nodev", dir->name, fs);
#endif

        return 0;
    }


    return errno = EINVAL, -1;
}


inode_t* vfs_open(inode_t* inode, int flags) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.open)) {
        scoped_lock(&inode->lock) return inode->ops.open(inode, flags);
    }

    return errno = ENOSYS, NULL;
}


int vfs_close(inode_t* inode) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.close)) {

        scoped_lock(&inode->lock) {
            int e = inode->ops.close(inode);

            shared_ptr_nullable_access(inode->ev, ev, {
                atomic_fetch_add(&ev->futex, 1);
            });

            return e;
        }
    }

    return errno = ENOSYS, -1;
}


/**
 * @brief Frees an inode nothing references any more, with what its filesystem keeps for it, and unpins its directory.
 *
 * @param inode The inode to free.
 */
static void __vfs_inode_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    inode_t* parent = (atomic_load(&inode->refcount) & INODE_REF_UNLINKED) ? inode->parent : NULL;

    if (inode->ops.release)
        inode->ops.release(inode);

    vfs_dcache_free(inode);

    if (inode->ev) {
        shared_ptr_free(inode->ev);
        inode->ev = NULL;
    }

    kfree(inode);

    if (parent)
        vfs_inode_put(parent);
}


/**
 * @brief Takes a reference to an inode the caller can already reach.
 *
 * @param inode The inode.
 * @return The inode.
 */
inode_t* vfs_inode_get(inode_t* inode) {

    DEBUG_ASSERT(inode);

    atomic_fetch_add(&inode->refcount, 1);

    return inode;
}


/**
 * @brief Drops a reference taken with vfs_inode_get(), freeing an unlinked or anonymous inode on the last one.
 *
 * @param inode The inode.
 */
void vfs_inode_put(inode_t* inode) {

    DEBUG_ASSERT(inode);

    unsigned int old = atomic_fetch_sub(&inode->refcount, 1);

    DEBUG_ASSERT((old & ~INODE_REF_UNLINKED) > 0);

    if ((old & ~INODE_REF_UNLINKED) != 1)
        return;

    if ((old & INODE_REF_UNLINKED) || (inode->flags & INODE_FLAGS_ANONYMOUS))
        __vfs_inode_release(inode);
}


/**
 * @brief Marks an inode's directory entry gone, freeing it now if nothing holds it and pinning its directory otherwise.
 *
 * @param inode The inode whose entry was removed.
 */
void vfs_inode_unlink(inode_t* inode) {

    DEBUG_ASSERT(inode);

    if (inode->parent)
        vfs_inode_get(inode->parent);

    unsigned int old = atomic_fetch_or(&inode->refcount, INODE_REF_UNLINKED);

    if (unlikely(old & INODE_REF_UNLINKED)) {

        if (inode->parent)
            vfs_inode_put(inode->parent);

        return;
    }

    if (old == 0)
        __vfs_inode_release(inode);
}


/**
 * @brief Tells whether an inode's directory entry is gone.
 *
 * @param inode The inode.
 * @return true once the inode has been unlinked.
 */
bool vfs_inode_unlinked(inode_t* inode) {

    DEBUG_ASSERT(inode);

    return !!(atomic_load(&inode->refcount) & INODE_REF_UNLINKED);
}


/**
 * @brief Frees an anonymous inode that never got a holder, dropping the reference it holds on its change counter.
 *
 * @param inode The inode to free.
 */
void vfs_anonymous_free(inode_t* inode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->flags & INODE_FLAGS_ANONYMOUS);
    DEBUG_ASSERT(atomic_load(&inode->refcount) == 0);

    __vfs_inode_release(inode);
}


/**
 * @brief Moves the caller's working directory, and its root too if asked, onto an inode.
 *
 * @param inode The directory.
 * @param root Whether the root moves as well.
 */
void fs_chdir(inode_t* inode, bool root) {

    DEBUG_ASSERT(inode);

    inode_t* oldcwd  = NULL;
    inode_t* oldroot = NULL;

    vfs_inode_get(inode);

    if (root)
        vfs_inode_get(inode);

    shared_ptr_access(current_task->fs, fs, {
        oldcwd  = fs->cwd;
        fs->cwd = inode;

        if (root) {
            oldroot  = fs->root;
            fs->root = inode;
        }
    });

    if (oldcwd)
        vfs_inode_put(oldcwd);

    if (oldroot)
        vfs_inode_put(oldroot);
}


/**
 * @brief Records the executable the caller now runs.
 *
 * @param inode The executable.
 */
void fs_set_exe(inode_t* inode) {

    DEBUG_ASSERT(inode);

    inode_t* old = NULL;

    vfs_inode_get(inode);

    shared_ptr_access(current_task->fs, fs, {
        old     = fs->exe;
        fs->exe = inode;
    });

    if (old)
        vfs_inode_put(old);
}


/**
 * @brief Takes a reference to every inode a filesystem context that has just been copied points at.
 *
 * @param fs The copy.
 */
void fs_ref_all(struct fs* fs) {

    DEBUG_ASSERT(fs);

    if (fs->root)
        vfs_inode_get(fs->root);

    if (fs->cwd)
        vfs_inode_get(fs->cwd);

    if (fs->exe)
        vfs_inode_get(fs->exe);
}


/**
 * @brief Drops the references a filesystem context that nothing uses any more holds.
 *
 * @param fs The context.
 */
void fs_put_all(struct fs* fs) {

    DEBUG_ASSERT(fs);

    inode_t* held[] = {fs->root, fs->cwd, fs->exe};

    fs->root = NULL;
    fs->cwd  = NULL;
    fs->exe  = NULL;

    for (size_t i = 0; i < sizeof(held) / sizeof(held[0]); i++) {

        if (held[i])
            vfs_inode_put(held[i]);
    }
}


int vfs_ioctl(inode_t* inode, long req, void* arg) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.ioctl)) {
        scoped_lock(&inode->lock) return inode->ops.ioctl(inode, req, arg);
    }

    return errno = ENOSYS, -1;
}


void vfs_notify(inode_t* inode) {

    if (unlikely(!inode))
        return;

    shared_ptr_nullable_access(inode->ev, ev, {
        atomic_fetch_add(&ev->futex, 1);
    });
}


int vfs_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.poll)) {
        scoped_lock(&inode->lock) return inode->ops.poll(inode, events);
    }

    return events & (POLLIN | POLLOUT);
}


int vfs_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    if (likely(inode->ops.getattr)) {
        scoped_lock(&inode->lock) return inode->ops.getattr(inode, st);
    }

    return errno = ENOSYS, -1;
}


int vfs_setattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    if (likely(inode->ops.setattr)) {
        scoped_lock(&inode->lock) return inode->ops.setattr(inode, st);
    }

    return errno = EROFS, -1;
}


int vfs_setxattr(inode_t* inode, const char* name, const void* value, size_t size, int flags) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.setxattr)) {
        scoped_lock(&inode->lock) return inode->ops.setxattr(inode, name, value, size, flags);
    }

    return errno = EROFS, -1;
}


int vfs_getxattr(inode_t* inode, const char* name, void* value, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(value);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.getxattr)) {
        scoped_lock(&inode->lock) return inode->ops.getxattr(inode, name, value, size);
    }

    return errno = ENOSYS, -1;
}


int vfs_listxattr(inode_t* inode, char* buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.listxattr)) {
        scoped_lock(&inode->lock) return inode->ops.listxattr(inode, buf, size);
    }

    return errno = ENOSYS, -1;
}


int vfs_removexattr(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if (likely(inode->ops.removexattr)) {
        scoped_lock(&inode->lock) return inode->ops.removexattr(inode, name);
    }

    return errno = EROFS, -1;
}


int vfs_truncate(inode_t* inode, off_t len) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.truncate)) {
        scoped_lock(&inode->lock) return inode->ops.truncate(inode, len);
    }

    return errno = EROFS, -1;
}


int vfs_fsync(inode_t* inode, int datasync) {

    DEBUG_ASSERT(inode);

    if (likely(inode->ops.fsync)) {
        scoped_lock(&inode->lock) return inode->ops.fsync(inode, datasync);
    }

    return errno = ENOSYS, -1;
}


ssize_t vfs_read(inode_t* inode, void* buf, off_t off, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.read)) {

        scoped_lock(&inode->lock) {

            ssize_t e = inode->ops.read(inode, buf, off, size);

            if (e > 0) {

                shared_ptr_nullable_access(inode->ev, ev, {
                    atomic_fetch_add(&ev->futex, 1);
                });
            }

            return e;
        }
    }

    return -ENOSYS;
}


/**
 * @brief Writes to an inode whose lock the caller holds, and signals its readers.
 *
 * @param inode The inode, locked.
 * @param buf The data.
 * @param off The offset.
 * @param size The number of bytes to write.
 * @return What the filesystem's write returned.
 */
static ssize_t vfs_write_locked(inode_t* inode, const void* buf, off_t off, size_t size) {

    ssize_t e = inode->ops.write(inode, buf, off, size);

    if (e > 0) {

        shared_ptr_nullable_access(inode->ev, ev, {
            atomic_fetch_add(&ev->futex, 1);
        });
    }

    return e;
}


ssize_t vfs_write(inode_t* inode, const void* buf, off_t off, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if (likely(inode->ops.write)) {
        scoped_lock(&inode->lock) return vfs_write_locked(inode, buf, off, size);
    }

    return -ENOSYS;
}


/**
 * @brief Writes a vector in one hold of the inode's lock, so no other write lands between its buffers.
 *
 * @param inode The inode.
 * @param iov The buffers, none of them empty.
 * @param count How many buffers there are.
 * @param off Where to write, ignored for a regular file when @p append is set; receives the offset written at.
 * @param append Whether a regular file is written at its end, read under the same lock.
 * @return The number of bytes written, what the filesystem's write returned when it wrote none, or -ENOSYS.
 */
ssize_t vfs_writev(inode_t* inode, const struct iovec* iov, size_t count, off_t* off, bool append) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(iov);
    DEBUG_ASSERT(count);
    DEBUG_ASSERT(off);


    if (likely(inode->ops.write)) {

        scoped_lock(&inode->lock) {

            struct stat st;

            if (append && inode->ops.getattr && inode->ops.getattr(inode, &st) == 0 && S_ISREG(st.st_mode))
                *off = st.st_size;


            ssize_t done = 0;
            ssize_t e    = 0;

            for (size_t i = 0; i < count; i++) {

                DEBUG_ASSERT(iov[i].iov_base);
                DEBUG_ASSERT(iov[i].iov_len);

                if ((e = vfs_write_locked(inode, iov[i].iov_base, *off + done, iov[i].iov_len)) <= 0)
                    break;

                done += e;

                if ((size_t)e < iov[i].iov_len)
                    break;
            }

            return done > 0 ? done : e;
        }
    }

    return -ENOSYS;
}


/**
 * @brief Writes at the end of a regular file, reading its size under the same lock as the write; anything else is written at @p off.
 *
 * @param inode The inode.
 * @param buf The data.
 * @param off The offset for an inode that is not a regular file; receives the offset written at.
 * @param size The number of bytes to write.
 * @return What the filesystem's write returned, or -ENOSYS.
 */
ssize_t vfs_write_append(inode_t* inode, const void* buf, off_t* off, size_t size) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    struct iovec iov = {
        .iov_base = (void*)buf,
        .iov_len  = size,
    };

    return vfs_writev(inode, &iov, 1, off, true);
}


ssize_t vfs_readlink(inode_t* inode, char* buf, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.readlink)) {
        scoped_lock(&inode->lock) return inode->ops.readlink(inode, buf, size);
    }

    return errno = ENOSYS, -1;
}


/**
 * @brief Creates an entry in a directory, or finds the one already there under that name.
 *
 * @param inode The directory.
 * @param name The name of the entry.
 * @param mode The type and permissions of a new entry.
 * @return The inode, referenced, or NULL with errno set.
 */
inode_t* vfs_creat(inode_t* inode, const char* name, mode_t mode) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if (name[0] == '.' && name[1] == '\0')
        return errno = EEXIST, NULL;

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EEXIST, NULL;



    if (unlikely(vfs_inode_unlinked(inode)))
        return errno = ENOENT, NULL;


    if (likely(inode->ops.creat)) {

        scoped_lock(&inode->lock) {

            inode_t* r = vfs_dcache_find(inode, name);

            if (unlikely(r))
                return r;


            if ((r = inode->ops.creat(inode, name, mode)) != NULL) {

                if (likely(r->parent == inode))
                    r = vfs_inode_get(vfs_dcache_add(inode, r));
            }

            return r;
        }
    }


    return errno = ENOSYS, NULL;
}


/**
 * @brief Looks a name up in a directory.
 *
 * @param inode The directory.
 * @param name The name to look for, which may be "." or "..".
 * @return The inode, referenced, or NULL with errno set.
 */
inode_t* vfs_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);


    if (name[0] == '.' && name[1] == '\0')
        return vfs_inode_get(inode);


    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {

        inode_t* parent = NULL;

        shared_ptr_access(current_task->fs, fs, {
            if (fs->root != inode || inode->parent)
                parent = inode->parent;
            else
                errno = ENOENT;
        });

        return parent ? vfs_inode_get(parent) : NULL;
    }



    inode_t* r = NULL;

    if ((r = vfs_dcache_find(inode, name)) != NULL)
        return r;


    if (likely(inode->ops.finddir)) {

        scoped_lock(&inode->lock) {

            if ((r = vfs_dcache_find(inode, name)) != NULL)
                return r;


            if ((r = inode->ops.finddir(inode, name)) != NULL) {

                if (likely(r->parent == inode))
                    r = vfs_inode_get(vfs_dcache_add(inode, r));
            }
        }

        return r;
    }

    return errno = ENOSYS, NULL;
}


ssize_t vfs_readdir(inode_t* inode, struct dirent* ent, off_t off, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(ent);
    DEBUG_ASSERT(size);

    if (likely(inode->ops.readdir)) {
        scoped_lock(&inode->lock) return inode->ops.readdir(inode, ent, off, size);
    }

    return errno = ENOSYS, -1;
}


int vfs_rename(inode_t* inode, const char* name, const char* newname) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(newname);


    if (name[0] == '.' && name[1] == '\0')
        return errno = EINVAL, -1;

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EINVAL, -1;

    if (newname[0] == '.' && newname[1] == '\0')
        return errno = EEXIST, -1;

    if (newname[0] == '.' && newname[1] == '.' && newname[2] == '\0')
        return errno = EEXIST, -1;



    if (likely(inode->ops.rename)) {
        scoped_lock(&inode->lock) return inode->ops.rename(inode, name, newname);
    }

    return errno = EROFS, -1;
}


int vfs_symlink(inode_t* inode, const char* name, const char* target) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(target);


    if (name[0] == '.' && name[1] == '\0')
        return errno = EEXIST, -1;

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = EEXIST, -1;



    if (likely(inode->ops.symlink)) {
        scoped_lock(&inode->lock) return inode->ops.symlink(inode, name, target);
    }

    return errno = EROFS, -1;
}


int vfs_unlink(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);


    if (name[0] == '.' && name[1] == '\0')
        return errno = ENOTEMPTY, -1;

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
        return errno = ENOTEMPTY, -1;



    if (likely(inode->ops.unlink)) {

        int r = -1;

        scoped_lock(&inode->lock) {

            if ((r = inode->ops.unlink(inode, name)) == 0)
                vfs_dcache_remove(inode, name);
        }

        return r;
    }

    return errno = EROFS, -1;
}
