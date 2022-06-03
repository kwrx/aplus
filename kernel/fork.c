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


#define __refcount_get(p)   \
    ({ p->refcount++; p; })




#define __dup(property, param)                                      \
    static void* __dup_##property (param c) {                       \
        param r = (param) kcalloc(1, sizeof(*c), GFP_KERNEL);       \
        memcpy(r, c, sizeof(*c));                                   \
        r->refcount = 1;                                            \
        return r;                                                   \
    }


__dup(fs, struct fs*)
__dup(sighand, struct sighand*)




static void* __dup_fd(struct fd* c) {

    struct fd* r = (struct fd*) kcalloc(1, sizeof(struct fd), GFP_KERNEL);

    memcpy(r, c, sizeof(*c));


    int i;
    for(i = 0; i < CONFIG_OPEN_MAX; i++) {
        
        if(!r->descriptors[i].ref)
            continue;

        fd_ref(r->descriptors[i].ref);

    }


    r->refcount = 1;
    return r;

}

static void* __dup_address_space(vmm_address_space_t* space) {

    vmm_address_space_t* r = (vmm_address_space_t*) kcalloc(1, sizeof(vmm_address_space_t), GFP_KERNEL);


#if defined(CONFIG_DEMAND_PAGING)
    arch_vmm_clone(r, space, ARCH_VMM_CLONE_DEMAND);
#else
    arch_vmm_clone(r, space, 0);
#endif

    arch_task_switch_address_space(NULL);

    r->refcount = 1;
    return r;

}



void do_unshare(int flags) {

    #define __clone_property(flag, property) {                                      \
        if(flags & flag) {                                                          \
            if(--current_task->property->refcount > 0)                              \
                current_task->property = __dup_##property (current_task->property); \
            else                                                                    \
                current_task->property->refcount = 1;                               \
        }                                                                           \
    }

    __clone_property(CLONE_FILES, fd);
    __clone_property(CLONE_FS, fs);
    __clone_property(CLONE_SIGHAND, sighand);
    __clone_property(CLONE_VM, address_space);

    #undef __clone_property


    if(flags & CLONE_VM)
        arch_task_switch_address_space(current_task->address_space);

}



pid_t do_fork(struct kclone_args* args, size_t size) {

    DEBUG_ASSERT(args);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size == sizeof(struct kclone_args));


    if(args->stack == 0ULL) {

        if(unlikely(args->stack_size > 0ULL))
            return errno = EINVAL, -1;

    } else {

        // if(unlikely(args->stack_size == 0ULL))
        //     return errno = EINVAL, -1;

        if(unlikely(!uio_check(args->stack, R_OK | W_OK)))
            return errno = EFAULT, -1;

    }


    if(unlikely((args->flags & (CLONE_DETACHED | CSIGNAL)) == (CLONE_DETACHED | CSIGNAL)))
        return errno = EINVAL, -1;

    if(unlikely(((args->flags & (CLONE_THREAD | CLONE_PARENT)) == (CLONE_THREAD | CLONE_PARENT)) && args->exit_signal))
        return errno = EINVAL, -1;


    // TODO: implements CLONE_VFORK
    if((unlikely(args->flags & CLONE_VFORK)))
        return errno = ENOSYS, -1;

    

    task_t* child = arch_task_get_empty_thread(0);

    memcpy(&child->userspace, &current_task->userspace, sizeof(child->userspace));
    memcpy(&child->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);



    if(args->flags & CLONE_PARENT)
        child->parent = current_task->parent;

    if(args->flags & CLONE_SETTLS)
        child->userspace.thread_area = args->tls;

    if(args->flags & CLONE_THREAD)
        child->tgid = current_task->tgid;



    #define __clone_property(flag, property) {                          \
        if(args->flags & flag)                                          \
            child->property = __refcount_get(current_task->property);   \
        else                                                            \
            child->property = __dup_##property (current_task->property);\
    }


    __clone_property(CLONE_FILES, fd);
    __clone_property(CLONE_FS, fs);
    __clone_property(CLONE_SIGHAND, sighand);
    __clone_property(CLONE_VM, address_space);

    #undef __clone_property



    child->ustack = args->stack 
        ? (void*) args->stack 
        : (void*) current_cpu->ustack
        ;


    arch_task_context_set(child, ARCH_TASK_CONTEXT_COPY, (long) current_cpu->frame);
    arch_task_context_set(child, ARCH_TASK_CONTEXT_RETVAL, 0L);

    
    sched_enqueue(child);

    return child->tid;

}