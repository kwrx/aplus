/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
__dup(fd, struct fd*)
__dup(sighand, struct sighand*)


static void* __dup_address_space(vmm_address_space_t* space) {

    vmm_address_space_t* r = (vmm_address_space_t*) kcalloc(1, sizeof(vmm_address_space_t), GFP_KERNEL);

#if defined(CONFIG_DEMAND_PAGING)
    arch_vmm_clone(r, space, ARCH_VMM_CLONE_DEMAND);
#else
    arch_vmm_clone(r, space, 0);
#endif

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

    #undef __clone_property

}



pid_t do_fork(struct kclone_args* args, size_t size) {

    DEBUG_ASSERT(args);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(size == sizeof(struct kclone_args));


    if(args->stack == 0ULL) {

        if(unlikely(args->stack_size > 0ULL))
            return -EINVAL;

    } else {

        if(unlikely(args->stack_size == 0ULL))
            return -EINVAL;

        if(unlikely(!uio_check(args->stack, R_OK | W_OK)))
            return -EFAULT;

    }


    if(unlikely(args->flags & (CLONE_DETACHED | CSIGNAL)))
        return -EINVAL;

    if(unlikely((args->flags & (CLONE_THREAD | CLONE_PARENT)) && args->exit_signal))
        return -EINVAL;


    // TODO: implement CLONE_VFORK
    if((unlikely(args->flags & CLONE_VFORK)))
        return -ENOSYS;

    

    task_t* child = arch_task_get_empty_thread(0);

    memcpy(&child->userspace, &current_task->userspace, sizeof(child->userspace));
    memcpy(&child->rlimits, &current_task->rlimits, sizeof(struct rlimit) * RLIM_NLIMITS);



    if(args->flags & CLONE_PARENT)
        child->parent = current_task->parent;

    if(args->flags & CLONE_SETTLS)
        child->userspace.thread_area = args->tls;

    // if(args->flags & CLONE_STOPPED)
    //     child->status = TASK_STATUS_STOP;

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



    child->sp3 = current_cpu->sp3;
    

    arch_task_context_set(child, ARCH_TASK_CONTEXT_COPY, (long) current_cpu->frame);
    arch_task_context_set(child, ARCH_TASK_CONTEXT_RETVAL, 0L);


    sched_enqueue(child);

    return child->tid;

}