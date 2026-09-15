/*
 * Author(s):
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

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/shm.h>
#include <aplus/smp.h>
#include <aplus/task.h>


typedef struct {

    //? A slot is in use from the moment shm_get() claims it. It is not usable until @frames
    //? is filled in, though: the frames are allocated with the table unlocked, so that a
    //? multi-megabyte segment does not hold every other CPU off with interrupts disabled for
    //? the length of the allocation. A lookup therefore treats a slot with no frames as if it
    //? were not there.
    bool used;

    int id;
    key_t key;

    size_t size;
    size_t pages;
    uintptr_t* frames;

    size_t attaches;

    //? IPC_RMID has been seen. The segment is gone as far as shmget(2) and shmat(2) are
    //? concerned, but the frames stay until whoever still has it attached lets go -- which is
    //? what lets a display server hand a surface to a client and forget about it.
    bool removed;

    mode_t mode;

    uid_t uid;
    uid_t cuid;
    gid_t gid;
    gid_t cgid;

    pid_t cpid;
    pid_t lpid;

    time_t atime;
    time_t dtime;
    time_t ctime;

} shm_segment_t;


static shm_segment_t shm_segments[SHM_SEGMENT_MAX];

//? Bumped every time a slot is handed out, and folded into the id so that an id outlives the
//? segment it named: shmat(2) on a stale id fails rather than landing on a stranger's memory.
static uint32_t shm_slot_sequence[SHM_SEGMENT_MAX];

static size_t shm_total;

static spinlock_t shm_lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_CPU_OWNER);


static shm_segment_t* shm_lookup_locked(int id) {

    if (unlikely(id < 0))
        return NULL;


    shm_segment_t* seg = &shm_segments[(size_t)id % SHM_SEGMENT_MAX];

    if (!seg->used || !seg->frames || seg->id != id)
        return NULL;

    return seg;
}


/* The System V access check: the segment's own mode bits, rotated so that whichever class the
 * caller falls into lines up with the owner bits, and then matched against what was asked for.
 * @want is therefore always expressed in owner terms (S_IRUSR, S_IWUSR).
 */
static bool shm_permitted(const shm_segment_t* seg, mode_t want) {

    DEBUG_ASSERT(current_task);

    if (current_task->euid == 0)
        return true;


    mode_t granted = seg->mode;

    if (current_task->euid != seg->uid && current_task->euid != seg->cuid) {

        if (current_task->egid == seg->gid || current_task->egid == seg->cgid)
            granted <<= 3;
        else
            granted <<= 6;
    }

    return (granted & want) == want;
}


/* shmget(2) takes the access it wants in the same nine bits open(2) uses for a file mode, one
 * group per class. Which class the caller belongs to is decided by shm_permitted(), so fold
 * the three down onto one and line that up with the owner bits it matches against.
 */
static inline mode_t shm_requested_mode(int flags) {
    return (mode_t)(((((unsigned)flags >> 6) | ((unsigned)flags >> 3) | (unsigned)flags) & 07u) << 6);
}


/* The frame arrays below are only produced when a segment actually went away, so most calls
 * have nothing to release -- and kfree() asserts rather than tolerating a null pointer.
 */
static inline void shm_free_frames(uintptr_t* frames) {

    if (frames)
        kfree(frames);
}


/* Hand the frames back and free the slot. The caller must hold the table lock; the frame array
 * itself is returned rather than freed, because kfree() of a segment's worth of pointers is
 * pointless work to do with interrupts off.
 */
static uintptr_t* shm_destroy_locked(shm_segment_t* seg) {

    DEBUG_ASSERT(seg->attaches == 0);

    uintptr_t* frames = seg->frames;

    for (size_t i = 0; i < seg->pages; i++) {

        if (frames[i])
            pmm_free_block(frames[i]);
    }

    DEBUG_ASSERT(shm_total >= seg->size);

    shm_total -= seg->size;

    memset(seg, 0, sizeof(*seg));

    return frames;
}


/* Drop one attachment. Returns the frame array when that was the last one and the segment had
 * already been removed, so that the caller can free it outside the lock.
 */
static uintptr_t* shm_put_locked(int id) {

    shm_segment_t* seg = shm_lookup_locked(id);

    if (unlikely(!seg))
        return NULL;

    DEBUG_ASSERT(seg->attaches > 0);

    if (--seg->attaches)
        return NULL;

    if (!seg->removed)
        return NULL;

    return shm_destroy_locked(seg);
}


/* Look a segment up by key, or create one: the shmget(2) entry point.
 *
 * A lookup matches a segment at least @size bytes long. Asking for less than it holds is how a
 * second process attaches without caring how large the first one made it; asking for more would
 * go on to map past the end of it. Creating one of zero bytes is refused, since it has no frames
 * to share and no way to grow later.
 *
 * The slot is claimed under the table lock and its frames are allocated with the lock dropped: a
 * spinlock here disables interrupts, and a multi-megabyte segment is thousands of allocations
 * and as many page-sized memsets. The frames are zeroed because the segment is about to be
 * readable by another process. What makes the gap safe is that a slot with no frames yet is
 * invisible to every lookup, so a racing shmget() can neither take it nor see it half-built.
 *
 * Ids are a sequence number times the table size plus the slot, the classic System V scheme, so
 * a stale id fails rather than landing on whatever segment took the slot next. The sequence
 * starts at one and wraps short of overflowing an int, which keeps an id positive and never
 * zero -- callers keep these in structures that start out zeroed, and an id indistinguishable
 * from "none" is a trap.
 */
long shm_get(key_t key, size_t size, int flags) {

    DEBUG_ASSERT(current_task);


    const uintptr_t pagesize = arch_vmm_getpagesize();

    DEBUG_ASSERT(pagesize == PML1_PAGESIZE);


    if (unlikely(size > SHM_SEGMENT_SIZE_MAX))
        return -EINVAL;

    const size_t rounded = (size + pagesize - 1) & ~(pagesize - 1);


    if (key != IPC_PRIVATE) {

        scoped_lock(&shm_lock) {

            for (size_t i = 0; i < SHM_SEGMENT_MAX; i++) {

                shm_segment_t* seg = &shm_segments[i];

                if (!seg->used || !seg->frames || seg->removed || seg->key != key)
                    continue;


                if ((flags & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL))
                    return -EEXIST;

                if (rounded > seg->size)
                    return -EINVAL;

                if (!shm_permitted(seg, shm_requested_mode(flags)))
                    return -EACCES;


                return seg->id;
            }
        }

        if (!(flags & IPC_CREAT))
            return -ENOENT;
    }


    if (unlikely(rounded == 0))
        return -EINVAL;


    size_t slot = SHM_SEGMENT_MAX;

    scoped_lock(&shm_lock) {

        if (unlikely(shm_total + rounded > SHM_TOTAL_MAX || shm_total + rounded < shm_total))
            return -ENOSPC;


        for (size_t i = 0; i < SHM_SEGMENT_MAX; i++) {

            if (shm_segments[i].used)
                continue;

            slot = i;
            break;
        }

        if (unlikely(slot == SHM_SEGMENT_MAX))
            return -ENOSPC;


        shm_segment_t* seg = &shm_segments[slot];

        memset(seg, 0, sizeof(*seg));

        seg->used = true;

        uint32_t seq = ++shm_slot_sequence[slot];

        if (unlikely(seq > (uint32_t)(INT32_MAX / SHM_SEGMENT_MAX)))
            seq = shm_slot_sequence[slot] = 1;


        seg->id   = (int)((seq * SHM_SEGMENT_MAX) + slot);
        seg->key  = key;
        seg->size = rounded;

        shm_total += rounded;
    }


    const size_t pages = rounded / pagesize;

    uintptr_t* frames = (uintptr_t*)kcalloc(pages, sizeof(uintptr_t), GFP_KERNEL);

    size_t allocated = 0;

    if (likely(frames)) {

        for (; allocated < pages; allocated++) {

            uintptr_t frame = pmm_alloc_block();

            if (unlikely(frame == PMM_INVALID_ADDRESS))
                break;

            memset((void*)arch_vmm_p2v(frame, ARCH_VMM_AREA_HEAP), 0, pagesize);

            frames[allocated] = frame;
        }
    }


    if (unlikely(!frames || allocated != pages)) {

        for (size_t i = 0; i < allocated; i++)
            pmm_free_block(frames[i]);

        shm_free_frames(frames);

        scoped_lock(&shm_lock) {

            shm_total -= rounded;

            memset(&shm_segments[slot], 0, sizeof(shm_segment_t));
        }

        return -ENOMEM;
    }


    int id;

    scoped_lock(&shm_lock) {

        shm_segment_t* seg = &shm_segments[slot];

        seg->pages  = pages;
        seg->frames = frames;

        seg->mode = (mode_t)(flags & (S_IRWXU | S_IRWXG | S_IRWXO));

        seg->uid  = current_task->euid;
        seg->cuid = current_task->euid;
        seg->gid  = current_task->egid;
        seg->cgid = current_task->egid;

        seg->cpid = current_task->pid;

        seg->ctime = (time_t)arch_timer_gettime();

        id = seg->id;
    }

    return id;
}


/* Map a segment into the calling address space: the shmat(2) entry point.
 *
 * @addr must be zero. Placing a segment where the caller asks would need the fixed mapping
 * support mmap(2) is still missing, so the kernel picks -- out of the same window and the same
 * cursor sys_mmap() carves from, so that the two cannot hand out overlapping ranges. Like an
 * mmap, a detached range is not given back to the cursor: the window is 128GiB and the
 * alternative is a free list. SHM_RND is therefore meaningless here and SHM_REMAP is refused.
 *
 * A segment marked for removal takes no new attachment however many it already has, which is
 * what makes "create it, hand it over, forget it" safe: the last holder destroys it.
 *
 * The reference is taken before the mapping is built, so that the frames cannot be handed back
 * underneath it, and given back on every failure path below. The mapping itself is one call per
 * page: ARCH_VMM_MAP_FIXED walks a single physical run, and these frames were allocated one at
 * a time.
 *
 * @return the address the segment was mapped at, or a negative errno.
 */
long shm_attach(int id, uintptr_t addr, int flags) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->address_space);


    if (unlikely(addr != 0UL))
        return -EINVAL;

    if (unlikely(flags & SHM_REMAP))
        return -EINVAL;


    const bool readonly = (flags & SHM_RDONLY) != 0;

    vmm_address_space_t* space = current_task->address_space;

    const uintptr_t pagesize = arch_vmm_getpagesize();


    /* Initialised: scoped_lock() is a loop to the compiler, which cannot see that it always
       runs its body once. */
    size_t size       = 0;
    size_t pages      = 0;
    uintptr_t* frames = NULL;

    scoped_lock(&shm_lock) {

        shm_segment_t* seg = shm_lookup_locked(id);

        if (unlikely(!seg))
            return -EINVAL;

        if (unlikely(seg->removed))
            return -EIDRM;

        if (unlikely(!shm_permitted(seg, readonly ? S_IRUSR : (S_IRUSR | S_IWUSR))))
            return -EACCES;


        seg->attaches++;

        size   = seg->size;
        pages  = seg->pages;
        frames = seg->frames;
    }


    uintptr_t start = 0UL;
    size_t slot     = SHM_ATTACH_MAX;

    scoped_lock(&space->lock) {

        for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

            if (space->shm.attachments[i].addr)
                continue;

            slot = i;
            break;
        }

        if (likely(slot != SHM_ATTACH_MAX)) {

            uintptr_t cursor = space->mmap.heap_end;

            if (cursor & (pagesize - 1))
                cursor = (cursor & ~(pagesize - 1)) + pagesize;


            if (likely(cursor + size > cursor && cursor + size <= space->mmap.heap_limit)) {

                space->shm.attachments[slot].addr = cursor;
                space->shm.attachments[slot].size = size;
                space->shm.attachments[slot].id   = id;

                space->mmap.heap_end = cursor + size;

                start = cursor;

            } else {

                slot = SHM_ATTACH_MAX;
            }
        }
    }


    if (unlikely(slot == SHM_ATTACH_MAX)) {

        uintptr_t* stale = NULL;

        scoped_lock(&shm_lock) {
            stale = shm_put_locked(id);
        }

        shm_free_frames(stale);

        return -ENOMEM;
    }


    int arch_flags = ARCH_VMM_MAP_USER | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_TYPE_SHARED;

    if (!readonly)
        arch_flags |= ARCH_VMM_MAP_RDWR;


    for (size_t i = 0; i < pages; i++) {

        if (likely(arch_vmm_map(space, start + (i * pagesize), frames[i], pagesize, arch_flags) != ARCH_VMM_MAP_FAILED))
            continue;


        if (i)
            arch_vmm_unmap(space, start, i * pagesize);

        scoped_lock(&space->lock) {
            memset(&space->shm.attachments[slot], 0, sizeof(shm_attach_t));
        }

        uintptr_t* stale = NULL;

        scoped_lock(&shm_lock) {
            stale = shm_put_locked(id);
        }

        shm_free_frames(stale);

        return -ENOMEM;
    }


    scoped_lock(&shm_lock) {

        shm_segment_t* seg = shm_lookup_locked(id);

        if (likely(seg)) {

            seg->lpid  = current_task->pid;
            seg->atime = (time_t)arch_timer_gettime();
        }
    }

    return (long)start;
}


/* Unmap an attachment: the shmdt(2) entry point.
 *
 * Only the page table entries go. The frames are not this address space's to free, which is
 * exactly what ARCH_VMM_MAP_TYPE_SHARED tells arch_vmm_unmap(); they go back when the last
 * attachment to a removed segment is dropped, which may well be this one.
 */
long shm_detach(uintptr_t addr) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(current_task->address_space);


    vmm_address_space_t* space = current_task->address_space;

    size_t size = 0;
    int id      = -1;

    scoped_lock(&space->lock) {

        for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

            if (space->shm.attachments[i].addr != addr)
                continue;


            size = space->shm.attachments[i].size;
            id   = space->shm.attachments[i].id;

            memset(&space->shm.attachments[i], 0, sizeof(shm_attach_t));

            break;
        }
    }

    if (unlikely(id < 0))
        return -EINVAL;


    arch_vmm_unmap(space, addr, size);


    uintptr_t* stale = NULL;

    scoped_lock(&shm_lock) {

        shm_segment_t* seg = shm_lookup_locked(id);

        if (likely(seg)) {

            seg->lpid  = current_task->pid;
            seg->dtime = (time_t)arch_timer_gettime();
        }

        stale = shm_put_locked(id);
    }

    shm_free_frames(stale);

    return 0;
}


/* Query or change a segment: the shmctl(2) entry point, for IPC_STAT, IPC_SET and IPC_RMID.
 *
 * IPC_RMID on a segment nobody holds destroys it there and then; on one that is still attached
 * it only marks it, and the frames go back when the last holder detaches. That is what makes it
 * safe to remove a segment the moment it has been handed to whoever is going to use it.
 */
long shm_control(int id, int cmd, struct shmid_ds* buf) {

    DEBUG_ASSERT(current_task);


    switch (cmd) {

        case IPC_STAT: {

            if (unlikely(!buf))
                return -EFAULT;

            if (unlikely(!uio_check(buf, R_OK | W_OK)))
                return -EFAULT;


            struct shmid_ds ds;

            memset(&ds, 0, sizeof(ds));

            scoped_lock(&shm_lock) {

                shm_segment_t* seg = shm_lookup_locked(id);

                if (unlikely(!seg))
                    return -EINVAL;

                if (unlikely(!shm_permitted(seg, S_IRUSR)))
                    return -EACCES;


                ds.shm_perm.__ipc_perm_key = seg->key;
                ds.shm_perm.uid            = seg->uid;
                ds.shm_perm.gid            = seg->gid;
                ds.shm_perm.cuid           = seg->cuid;
                ds.shm_perm.cgid           = seg->cgid;
                ds.shm_perm.mode           = seg->mode | (seg->removed ? SHM_DEST : 0);

                ds.shm_segsz  = seg->size;
                ds.shm_atime  = seg->atime;
                ds.shm_dtime  = seg->dtime;
                ds.shm_ctime  = seg->ctime;
                ds.shm_cpid   = seg->cpid;
                ds.shm_lpid   = seg->lpid;
                ds.shm_nattch = seg->attaches;
            }


            uio_lock(buf, sizeof(ds));

            uio_memcpy_s2u(buf, &ds, sizeof(ds));

            uio_unlock(buf, sizeof(ds));

            return 0;
        }

        case IPC_SET: {

            if (unlikely(!buf))
                return -EFAULT;

            if (unlikely(!uio_check(buf, R_OK)))
                return -EFAULT;


            struct shmid_ds ds;

            uio_lock(buf, sizeof(ds));

            uio_memcpy_u2s(&ds, buf, sizeof(ds));

            uio_unlock(buf, sizeof(ds));


            scoped_lock(&shm_lock) {

                shm_segment_t* seg = shm_lookup_locked(id);

                if (unlikely(!seg))
                    return -EINVAL;

                if (unlikely(current_task->euid != 0 && current_task->euid != seg->uid && current_task->euid != seg->cuid))
                    return -EPERM;


                seg->uid  = ds.shm_perm.uid;
                seg->gid  = ds.shm_perm.gid;
                seg->mode = ds.shm_perm.mode & (S_IRWXU | S_IRWXG | S_IRWXO);

                seg->ctime = (time_t)arch_timer_gettime();
            }

            return 0;
        }

        case IPC_RMID: {

            uintptr_t* stale = NULL;

            scoped_lock(&shm_lock) {

                shm_segment_t* seg = shm_lookup_locked(id);

                if (unlikely(!seg))
                    return -EINVAL;

                if (unlikely(current_task->euid != 0 && current_task->euid != seg->uid && current_task->euid != seg->cuid))
                    return -EPERM;


                seg->removed = true;

                if (!seg->attaches)
                    stale = shm_destroy_locked(seg);
            }

            shm_free_frames(stale);

            return 0;
        }
    }

    return -EINVAL;
}


/* @see include/aplus/shm.h. A missing segment cannot happen -- the parent holds a reference, and
 * a segment with one is never destroyed -- so there is nothing to inherit if it somehow did.
 */
void shm_address_space_clone(vmm_address_space_t* parent, vmm_address_space_t* dest) {

    scoped_lock(&shm_lock) {

        for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

            if (!parent->shm.attachments[i].addr)
                continue;


            shm_segment_t* seg = shm_lookup_locked(parent->shm.attachments[i].id);

            if (unlikely(!seg))
                continue;


            seg->attaches++;

            dest->shm.attachments[i] = parent->shm.attachments[i];
        }
    }
}


void shm_address_space_release(vmm_address_space_t* space) {

    for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

        if (!space->shm.attachments[i].addr)
            continue;


        const int id = space->shm.attachments[i].id;

        memset(&space->shm.attachments[i], 0, sizeof(shm_attach_t));


        uintptr_t* stale = NULL;

        scoped_lock(&shm_lock) {
            stale = shm_put_locked(id);
        }

        shm_free_frames(stale);
    }
}
