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

#include <stdint.h>                                                        
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/hal.h>
#include <aplus/errno.h>




static struct fs* dup_fs(const struct fs* fs) {
   
    DEBUG_ASSERT(fs);
    DEBUG_ASSERT(fs->refcount > 0);

    struct fs* new_fs = kcalloc(1, sizeof(struct fs), GFP_KERNEL);

    memcpy(new_fs, fs, sizeof(*fs));

    return new_fs->refcount = 1
         , new_fs;

}

static struct sighand* dup_sighand(const struct sighand* sighand) {
   
    DEBUG_ASSERT(sighand);
    DEBUG_ASSERT(sighand->refcount > 0);

    struct sighand* new_sighand = kcalloc(1, sizeof(struct sighand), GFP_KERNEL);

    memcpy(new_sighand, sighand, sizeof(*sighand));

    return new_sighand->refcount = 1
         , new_sighand;

}


static struct fd* dup_fd(const struct fd* fd) {

    DEBUG_ASSERT(fd);
    DEBUG_ASSERT(fd->refcount > 0);

    struct fd* new_fd = kcalloc(1, sizeof(struct fd), GFP_KERNEL);

    memcpy(new_fd, fd, sizeof(*fd));

    for(size_t i = 0; i < CONFIG_OPEN_MAX; i++) {
        
        if(!new_fd->descriptors[i].ref)
            continue;

        fd_ref(new_fd->descriptors[i].ref);

    }

    return new_fd->refcount = 1
         , new_fd;

}


/**
 * Allows a process to stop sharing certain resources with other processes in its thread group.
 *
 * @param flags an int that specifies which resources to stop sharing.
 * The following flags are valid:
 * <ul>
 * <li>CLONE_FILES - stop sharing file descriptors</li>
 * <li>CLONE_FS - stop sharing filesystem information</li>
 * <li>CLONE_SIGHAND - stop sharing signal handlers</li>
 * <li>CLONE_VM - stop sharing address space</li>
 * </ul>
 * 
 */
void do_unshare(int flags) {

    if(flags & CLONE_FILES) {

        if(--current_task->fd->refcount > 0) {
            current_task->fd = dup_fd(current_task->fd);
        } else {
            current_task->fd->refcount = 1;
        }
    }

    if(flags & CLONE_FS) {

        if(--current_task->fs->refcount > 0) {
            current_task->fs = dup_fs(current_task->fs);
        } else {
            current_task->fs->refcount = 1;
        }
    }

    if(flags & CLONE_SIGHAND) {

        if(--current_task->sighand->refcount > 0) {
            current_task->sighand = dup_sighand(current_task->sighand);
        } else {
            current_task->sighand->refcount = 1;
        }
    }

    if(flags & CLONE_VM) {

        int clone_flags = ARCH_VMM_CLONE_USERSPACE;

#if defined(CONFIG_DEMAND_PAGING)
        clone_flags |= ARCH_VMM_CLONE_DEMAND;
#endif

        if(--current_task->address_space->refcount > 0) {
            current_task->address_space = arch_vmm_create_address_space(current_task->address_space, clone_flags);
        } else {
            current_task->address_space->refcount = 1;
        }

        // Reload current address space
        arch_task_switch_address_space(NULL);

    }
    
}
    



/**
 * Creates a new task (a copy of the current task) with certain properties inherited or 
 * shared from the parent task based on the flags passed in the `kclone_args` struct.
 *
 * @param args  a pointer to a `kclone_args` struct containing the flags and 
 *              arguments for the fork system call.
 * @param size  the size of the `kclone_args` struct.
 * @return      the new task's process ID (`pid_t`), or -1 if an error occurred. 
 *              In case of error, `errno` is set to indicate the error.
*/
pid_t do_fork(struct kclone_args* args, size_t size) {

    DEBUG_ASSERT(args);
    DEBUG_ASSERT(size == sizeof(struct kclone_args));
 

    if(unlikely(!args)) {
        return errno = EINVAL, -1;
    }

    if(unlikely(size != sizeof(struct kclone_args))) {
        return errno = EINVAL, -1;
    }


    if(args->stack == 0ULL) {

        if(unlikely(args->stack_size > 0ULL)) {
            return errno = EINVAL, -1;
        }

    } else {

        // // if(unlikely(args->stack_size == 0ULL))
        // //     return errno = EINVAL, -1;

        if(unlikely(!uio_check(args->stack, R_OK | W_OK))) {
            return errno = EFAULT, -1;
        }

    }

    if(unlikely((args->flags & (CLONE_DETACHED | CSIGNAL)) == (CLONE_DETACHED | CSIGNAL))) {
        return errno = EINVAL, -1;
    }

    if(unlikely(((args->flags & (CLONE_THREAD | CLONE_PARENT)) == (CLONE_THREAD | CLONE_PARENT)) && args->exit_signal)) {
        return errno = EINVAL, -1;
    }


    // TODO: Implement CLONE_VFORK
    if((unlikely(args->flags & CLONE_VFORK))) {
        return errno = ENOSYS, -1;
    }

    
    task_t* child = arch_task_get_empty_thread(0);

    memcpy(&child->userspace, &current_task->userspace, sizeof(child->userspace));
    memcpy(&child->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);


    if(args->flags & CLONE_PARENT) {
        child->parent = current_task->parent;
    }

    if(args->flags & CLONE_SETTLS) {
        child->userspace.thread_area = args->tls;
    }

    if(args->flags & CLONE_THREAD) {
        child->tgid = current_task->tgid;
    }



    if(args->flags & CLONE_FILES) {
        child->fd = current_task->fd;
        child->fd->refcount += 1;
    } else {
        child->fd = dup_fd(current_task->fd);
    }

    if(args->flags & CLONE_FS) {
        child->fs = current_task->fs;
        child->fs->refcount += 1;
    } else {
        child->fs = dup_fs(current_task->fs);
    }

    if(args->flags & CLONE_SIGHAND) {
        child->sighand = current_task->sighand;
        child->sighand->refcount += 1;
    } else {
        child->sighand = dup_sighand(current_task->sighand);
    }

    if(args->flags & CLONE_VM) {
        child->address_space = current_task->address_space;
        child->address_space->refcount += 1;
    } else {

        int clone_flags = ARCH_VMM_CLONE_USERSPACE;

#if defined(CONFIG_DEMAND_PAGING)
        clone_flags |= ARCH_VMM_CLONE_DEMAND;
#endif

        child->address_space = arch_vmm_create_address_space(current_task->address_space, clone_flags);

    }



    child->ustack = args->stack 
        ? (void*) args->stack 
        : (void*) current_cpu->ustack
        ;


    arch_task_context_set(child, ARCH_TASK_CONTEXT_COPY, (long) current_cpu->frame);
    arch_task_context_set(child, ARCH_TASK_CONTEXT_RETVAL, 0L);

    
    sched_enqueue(child);


    return child->tid;

}