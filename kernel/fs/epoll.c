/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <fcntl.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/epoll.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/poll.h>
#include <aplus/task.h>
#include <aplus/vfs.h>


#define EPOLL_FSID      0xDEADCB01
#define EPOLL_FIRST_INO 0xFFFFFFFFE200000

#define EPOLL_ALWAYS (POLLERR | POLLHUP)


struct epoll_item {
    uint64_t id;
    int fd;
    struct file* file;
    uint64_t gen;
    uint32_t events;
    uint64_t data;
    uint32_t seq;
    bool fresh;
    bool disabled;
};

struct epoll_report {
    uint64_t id;
    uint32_t seq;
    bool stale;
};

struct epoll {
    struct epoll_item* items;
    size_t count;
    size_t slots;
    uint64_t nextid;
    spinlock_t lock;
};


static struct superblock epoll_superblock = {
    .fsid   = EPOLL_FSID,
    .flags  = ST_NOATIME | ST_NODEV | ST_NOSUID | ST_NOEXEC,
    .dev    = NULL,
    .root   = NULL,
    .ino    = EPOLL_FIRST_INO,
    .fsinfo = NULL,
    .st     = {.f_bsize = PML1_PAGESIZE, .f_frsize = PML1_PAGESIZE, .f_fsid = EPOLL_FSID, .f_namemax = CONFIG_MAXNAMLEN},
};

static _Atomic ino64_t __epoll_next_ino = EPOLL_FIRST_INO + 1;


static void __epoll_bump(inode_t* inode) {

    shared_ptr_nullable_access(inode->ev, ev, {
        atomic_fetch_add(&ev->futex, 1);
    });
}


static struct epoll_item* __epoll_snapshot(struct epoll* ep, size_t* count) {

    struct epoll_item* snap = NULL;

    *count = 0;

    scoped_lock(&ep->lock) {

        if (ep->count == 0) {
            break;
        }

        snap = (struct epoll_item*)kmalloc(ep->count * sizeof(struct epoll_item), GFP_ATOMIC);

        if (unlikely(!snap)) {
            break;
        }

        memcpy(snap, ep->items, ep->count * sizeof(struct epoll_item));

        *count = ep->count;
    }

    return snap;
}


static void __epoll_remove_locked(struct epoll* ep, size_t i) {

    ep->items[i] = ep->items[--ep->count];
}


static struct epoll_item* __epoll_find_locked(struct epoll* ep, uint64_t id, size_t* index) {

    for (size_t i = 0; i < ep->count; i++) {

        if (ep->items[i].id != id) {
            continue;
        }

        if (index) {
            *index = i;
        }

        return &ep->items[i];
    }

    return NULL;
}


static int __epoll_scan(struct epoll* ep, struct epoll_kevent* out, int max, bool consume) {

    size_t count = 0;

    struct epoll_item* snap = __epoll_snapshot(ep, &count);

    if (!snap) {
        return 0;
    }

    struct epoll_report* reports = consume ? (struct epoll_report*)kcalloc(count, sizeof(struct epoll_report), GFP_KERNEL) : NULL;

    if (unlikely(consume && !reports)) {
        kfree(snap);
        return -ENOMEM;
    }


    int ready      = 0;
    size_t nreport = 0;

    for (size_t i = 0; i < count && ready < max; i++) {

        struct epoll_item* it = &snap[i];

        if (it->disabled) {
            continue;
        }


        struct file* file = fd_get((unsigned int)it->fd, NULL);

        if (!file || file != it->file || file->gen != it->gen || !file->inode) {

            if (file) {
                fd_put(file);
            }

            if (consume) {
                reports[nreport++] = (struct epoll_report){.id = it->id, .stale = true};
            }

            continue;
        }


        volatile uint32_t* word = poll_event_word(file->inode);
        uint32_t seq            = word ? *word : 0;

        int revents = vfs_poll(file->inode, (short)(it->events & 0xFFFF)) & (int)((it->events & 0xFFFF) | EPOLL_ALWAYS);

        fd_put(file);


        if (!revents) {
            continue;
        }

        if ((it->events & EPOLL_KET) && !it->fresh && seq == it->seq) {
            continue;
        }

        if (out) {
            out[ready].events = (uint32_t)revents;
            out[ready].data   = it->data;
        }

        ready++;

        if (consume) {
            reports[nreport++] = (struct epoll_report){.id = it->id, .seq = seq, .stale = false};
        }
    }


    if (consume && nreport) {

        scoped_lock(&ep->lock) {

            for (size_t i = 0; i < nreport; i++) {

                size_t index          = 0;
                struct epoll_item* it = __epoll_find_locked(ep, reports[i].id, &index);

                if (!it) {
                    continue;
                }

                if (reports[i].stale) {
                    __epoll_remove_locked(ep, index);
                    continue;
                }

                it->seq   = reports[i].seq;
                it->fresh = false;

                if (it->events & EPOLL_KONESHOT) {
                    it->disabled = true;
                }
            }
        }
    }


    if (reports) {
        kfree(reports);
    }

    kfree(snap);

    return ready;
}


static int epoll_poll(inode_t* inode, int events) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    int ready = __epoll_scan((struct epoll*)inode->userdata, NULL, INT32_MAX, false);

    return ready > 0 ? (events & (POLLIN | POLLRDNORM)) : 0;
}


static int epoll_getattr(inode_t* inode, struct stat* st) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(st);

    memset(st, 0, sizeof(*st));

    st->st_ino     = inode->ino;
    st->st_mode    = 0600;
    st->st_nlink   = 1;
    st->st_blksize = PML1_PAGESIZE;

    st->st_mtime = arch_timer_gettime();
    st->st_atime = arch_timer_gettime();
    st->st_ctime = arch_timer_gettime();

    return 0;
}


static void epoll_release(inode_t* inode) {

    DEBUG_ASSERT(inode);

    struct epoll* ep = (struct epoll*)inode->userdata;

    if (ep) {

        if (ep->items) {
            kfree(ep->items);
        }

        kfree(ep);
    }

    inode->userdata = NULL;
}


bool epoll_is(inode_t* inode) {

    return inode && inode->sb == &epoll_superblock;
}


inode_t* epoll_inode(void) {

    inode_t* inode = (inode_t*)kcalloc(1, sizeof(inode_t), GFP_KERNEL);

    if (unlikely(!inode)) {
        return errno = ENOMEM, NULL;
    }

    struct epoll* ep = (struct epoll*)kcalloc(1, sizeof(struct epoll), GFP_KERNEL);

    if (unlikely(!ep)) {
        kfree(inode);
        return errno = ENOMEM, NULL;
    }

    inode->ev = shared_ptr_new(struct inode_events, GFP_KERNEL);

    if (unlikely(!inode->ev)) {
        kfree(ep);
        kfree(inode);
        return errno = ENOMEM, NULL;
    }


    ep->nextid = 1;
    spinlock_init(&ep->lock);

    strcpy(inode->name, "anon_inode:[eventpoll]");

    inode->ino      = atomic_fetch_add(&__epoll_next_ino, 1);
    inode->sb       = &epoll_superblock;
    inode->parent   = NULL;
    inode->flags    = INODE_FLAGS_ANONYMOUS;
    inode->userdata = ep;

    inode->ops.poll    = epoll_poll;
    inode->ops.getattr = epoll_getattr;
    inode->ops.release = epoll_release;

    spinlock_init(&inode->lock);

    return inode;
}


long epoll_ctl(inode_t* inode, int op, int fd, struct file* target, const struct epoll_kevent* event) {

    DEBUG_ASSERT(epoll_is(inode));
    DEBUG_ASSERT(target);

    struct epoll* ep = (struct epoll*)inode->userdata;

    if (unlikely(target->inode == inode || epoll_is(target->inode))) {
        return -EINVAL;
    }

    if (unlikely(!target->inode || !target->inode->ops.poll)) {
        return -EPERM;
    }

    if (unlikely(op != EPOLL_KCTL_DEL && !event)) {
        return -EFAULT;
    }


    long e = 0;

    scoped_lock(&ep->lock) {

        struct epoll_item* found = NULL;
        size_t index             = 0;

        for (size_t i = 0; i < ep->count;) {

            struct epoll_item* it = &ep->items[i];

            if (it->fd != fd) {
                i++;
                continue;
            }

            if (it->file != target || it->gen != target->gen) {
                __epoll_remove_locked(ep, i);
                continue;
            }

            found = it;
            index = i;
            break;
        }


        switch (op) {

            case EPOLL_KCTL_ADD:

                if (found) {
                    e = -EEXIST;
                    break;
                }

                if (ep->count == ep->slots) {

                    size_t slots = ep->slots ? ep->slots * 2 : 8;

                    struct epoll_item* items = (struct epoll_item*)krealloc(ep->items, slots * sizeof(struct epoll_item), GFP_ATOMIC);

                    if (unlikely(!items)) {
                        e = -ENOMEM;
                        break;
                    }

                    ep->items = items;
                    ep->slots = slots;
                }

                ep->items[ep->count++] = (struct epoll_item){
                    .id     = ep->nextid++,
                    .fd     = fd,
                    .file   = target,
                    .gen    = target->gen,
                    .events = event->events,
                    .data   = event->data,
                    .fresh  = true,
                };

                break;

            case EPOLL_KCTL_MOD:

                if (!found) {
                    e = -ENOENT;
                    break;
                }

                found->events   = event->events;
                found->data     = event->data;
                found->fresh    = true;
                found->disabled = false;

                break;

            case EPOLL_KCTL_DEL:

                if (!found) {
                    e = -ENOENT;
                    break;
                }

                __epoll_remove_locked(ep, index);

                break;

            default:
                e = -EINVAL;
                break;
        }
    }

    if (e == 0) {
        __epoll_bump(inode);
    }

    return e;
}


long epoll_wait(inode_t* inode, struct epoll_kevent* events, int maxevents, uint64_t timeout_ns) {

    DEBUG_ASSERT(epoll_is(inode));
    DEBUG_ASSERT(current_task);

    struct epoll* ep = (struct epoll*)inode->userdata;

    if (unlikely(maxevents <= 0 || maxevents > EPOLL_KMAXEVENTS)) {
        return -EINVAL;
    }

    if (unlikely(!events || !uio_check(events, W_OK) || !uio_check((uint8_t*)events + (size_t)maxevents * sizeof(struct epoll_kevent) - 1, W_OK))) {
        return -EFAULT;
    }


    size_t outsz             = (size_t)maxevents * sizeof(struct epoll_kevent);
    struct epoll_kevent* out = (struct epoll_kevent*)kmalloc(outsz, GFP_KERNEL);

    if (unlikely(!out)) {
        return -ENOMEM;
    }


    volatile uint32_t* word = poll_event_word(inode);
    uint32_t seq            = word ? *word : 0;

    long ready = __epoll_scan(ep, out, maxevents, true);

    if (ready != 0) {

        if (ready > 0) {
            uio_memcpy_s2u(events, out, (size_t)ready * sizeof(struct epoll_kevent));
        }

        kfree(out);
        return ready;
    }


    struct timespec tm  = {0, 0};
    struct timespec* to = NULL;

    switch (poll_deadline(timeout_ns, &tm)) {

        case POLL_DEADLINE_EXPIRED:
            kfree(out);
            return 0;

        case POLL_DEADLINE_REMAINING:
            to = &tm;
            break;

        case POLL_DEADLINE_FOREVER:
            break;
    }


    bool armed = false;

    if (word) {
        futex_wait(current_task, word, seq, to);
        armed = true;
    }


    size_t count = 0;

    struct epoll_item* snap = __epoll_snapshot(ep, &count);

    for (size_t i = 0; i < count; i++) {

        if (snap[i].disabled) {
            continue;
        }

        bool one = false;

        poll_arm(snap[i].fd, (short)(snap[i].events & 0xFFFF), to, &one);

        armed |= one;
    }

    if (snap) {
        kfree(snap);
    }


    if ((ready = __epoll_scan(ep, out, maxevents, true)) != 0) {

        if (ready > 0) {
            uio_memcpy_s2u(events, out, (size_t)ready * sizeof(struct epoll_kevent));
        }

        kfree(out);
        return ready;
    }

    kfree(out);

    return poll_suspend(armed, to);
}


long epoll_create_fd(bool cloexec) {

    inode_t* inode = epoll_inode();

    if (unlikely(!inode)) {
        return -ENOMEM;
    }

    struct file* ref = fd_append(inode, 0, O_RDWR);

    if (unlikely(!ref)) {
        vfs_anonymous_free(inode);
        return -ENFILE;
    }

    long fd = fd_install(ref, cloexec);

    if (unlikely(fd < 0)) {
        fd_remove(ref, true);
    }

    return fd;
}


long epoll_ctl_fd(int epfd, int op, int fd, const void* uevent) {

    if (unlikely(epfd < 0 || fd < 0)) {
        return -EBADF;
    }

    struct epoll_kevent event = {0};

    if (op != EPOLL_KCTL_DEL) {

        if (unlikely(!uevent || !uio_check(uevent, R_OK))) {
            return -EFAULT;
        }

        uio_memcpy_u2s(&event, uevent, sizeof(event));
    }


    struct file* epf = fd_get((unsigned int)epfd, NULL);

    if (unlikely(!epf)) {
        return -EBADF;
    }

    struct file* target = fd_get((unsigned int)fd, NULL);

    if (unlikely(!target)) {
        fd_put(epf);
        return -EBADF;
    }


    long e = epoll_is(epf->inode) ? epoll_ctl(epf->inode, op, fd, target, op == EPOLL_KCTL_DEL ? NULL : &event) : -EINVAL;

    fd_put(target);
    fd_put(epf);

    return e;
}


long epoll_wait_fd(int epfd, void* uevents, int maxevents, uint64_t timeout_ns) {

    if (unlikely(epfd < 0)) {
        return -EBADF;
    }

    struct file* epf = fd_get((unsigned int)epfd, NULL);

    if (unlikely(!epf)) {
        return -EBADF;
    }

    long e = epoll_is(epf->inode) ? epoll_wait(epf->inode, (struct epoll_kevent*)uevents, maxevents, timeout_ns) : -EINVAL;

    fd_put(epf);

    return e;
}
