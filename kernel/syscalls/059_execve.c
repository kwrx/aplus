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

#include <sched.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/elf.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus/hal.h>



#define AT_NULL          0  /* end of vector */
#define AT_IGNORE        1  /* entry should be ignored */
#define AT_EXECFD        2  /* file descriptor of program */
#define AT_PHDR          3  /* program headers for program */
#define AT_PHENT         4  /* size of program header entry */
#define AT_PHNUM         5  /* number of program headers */
#define AT_PAGESZ        6  /* system page size */
#define AT_BASE          7  /* base address of interpreter */
#define AT_FLAGS         8  /* flags */
#define AT_ENTRY         9  /* entry point of program */
#define AT_NOTELF        10 /* program is not ELF */
#define AT_UID           11 /* real uid */
#define AT_EUID          12 /* effective uid */
#define AT_GID           13 /* real gid */
#define AT_EGID          14 /* effective gid */
#define AT_PLATFORM      15 /* string identifying CPU for optimizations */
#define AT_HWCAP         16 /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK        17 /* frequency at which times() increments */
#define AT_SECURE        23 /* secure mode boolean */
#define AT_BASE_PLATFORM 24 /* string identifying real platform, may differ from AT_PLATFORM. */
#define AT_RANDOM        25 /* address of 16 random bytes */
#define AT_HWCAP2        26 /* extension of AT_HWCAP */
#define AT_EXECFN        31 /* filename of program */



/**
 * @brief Most program headers an image may have.
 */
#define EXECVE_PHNUM_MAX 128

/**
 * @brief Least room the arguments and environment always get, whatever the stack limit.
 */
#define EXECVE_ARG_MIN (128UL * 1024UL)


#define AUX_ENT(id, value)     \
    {                          \
        uio_wptr(sp++, id);    \
        uio_wptr(sp++, value); \
    }


/**
 * @brief Where execve() put the stacks of the program it is starting.
 */
struct execve_layout {

    uintptr_t stack;
    uintptr_t bottom;
    uintptr_t sigstack;
    uintptr_t siginfo;
};


/**
 * @brief Frees a kernel buffer when the variable holding it goes out of scope.
 *
 * @param p The variable.
 */
static inline void __execve_kfree(void* p) {

    void** q = (void**)p;

    if (*q)
        kfree(*q);
}


/**
 * @brief Grows the break of the address space being built.
 *
 * @param incr Bytes to add.
 * @param base Receives the old break, where the new memory starts.
 * @return 0 on success, or -ENOMEM.
 */
static long __execve_sbrk(uintptr_t incr, uintptr_t* base) {

    uintptr_t brk = (uintptr_t)sys_brk(0);

    if (incr && (uintptr_t)sys_brk(brk + incr) < brk + incr)
        return -ENOMEM;

    *base = brk;

    return 0;
}


/**
 * @brief Copies a user array of strings into a kernel buffer, or only measures it when @p buf is NULL.
 *
 * @param vec The NULL-terminated user array.
 * @param buf Where the strings go, one after the other, or NULL.
 * @param room The bytes the strings and a pointer to each may take.
 * @param count Receives how many strings there are.
 * @return The bytes the strings take, or -EFAULT or -E2BIG.
 */
static long __execve_copy_strings(const char* const* vec, char* buf, size_t room, size_t* count) {

    const uintptr_t pagesize = arch_vmm_getpagesize();

    size_t used = 0;
    size_t n    = 0;

    for (;; n++) {

        const char* const* slot = &vec[n];

        if (unlikely(!uio_check(slot, R_OK) || !uio_check((const char*)(slot + 1) - 1, R_OK)))
            return -EFAULT;

        const char* s = NULL;
        uio_memcpy_u2s(&s, slot, sizeof(s));

        if (!s)
            break;

        for (;;) {

            if (unlikely(!uio_check(s, R_OK)))
                return -EFAULT;

            const uintptr_t phys = arch_vmm_v2p((uintptr_t)s, ARCH_VMM_AREA_USER);

            if (unlikely(phys == ARCH_VMM_MAP_FAILED))
                return -EFAULT;

            const size_t chunk = pagesize - ((uintptr_t)s & (pagesize - 1));
            const char* k      = (const char*)arch_vmm_p2v(phys, ARCH_VMM_AREA_HEAP);
            const char* z      = memchr(k, '\0', chunk);
            const size_t take  = z ? (size_t)(z - k) + 1 : chunk;

            if (unlikely(used + take + (n + 1) * sizeof(char*) > room))
                return -E2BIG;

            if (buf)
                memcpy(buf + used, k, take);

            used += take;
            s += take;

            if (z)
                break;
        }
    }

    *count = n;

    return (long)used;
}


/**
 * @brief Checks that an image is a static executable whose segments fit below the mmap window.
 *
 * @param phdrs The program headers.
 * @param phnum How many.
 * @param limit The first address a segment may not reach.
 * @return 0, or -ENOEXEC.
 */
static long __execve_check(const Elf_Phdr* phdrs, size_t phnum, uintptr_t limit) {

    size_t loads = 0;

    for (size_t i = 0; i < phnum; i++) {

        const Elf_Phdr* ph = &phdrs[i];

        switch (ph->p_type) {

            case PT_DYNAMIC:
            case PT_INTERP:
                return -ENOEXEC;

            case PT_LOAD:

                if (ph->p_memsz == 0 || ph->p_filesz > ph->p_memsz)
                    return -ENOEXEC;

                if (ph->p_vaddr < arch_vmm_getpagesize() || ph->p_vaddr + ph->p_memsz < ph->p_vaddr || ph->p_vaddr + ph->p_memsz > limit)
                    return -ENOEXEC;

                if (ph->p_align & (ph->p_align - 1))
                    return -ENOEXEC;

                loads++;
                break;
        }
    }

    return loads ? 0 : -ENOEXEC;
}


/**
 * @brief Maps and reads the loadable segments of an image into the address space being built.
 *
 * @param inode The image.
 * @param head Its ELF header.
 * @param phdrs Its program headers, already checked.
 * @param space The address space being built, loaded on this CPU.
 * @param phdr_addr Receives where the program headers end up, or is left alone.
 * @return 0 on success, or a negative errno.
 */
static long __execve_load(inode_t* inode, const Elf_Ehdr* head, const Elf_Phdr* phdrs, vmm_address_space_t* space, uintptr_t* phdr_addr) {

    const uintptr_t pagesize = arch_vmm_getpagesize();

    for (size_t i = 0; i < head->e_phnum; i++) {

        const Elf_Phdr* ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;


        if (head->e_phoff >= ph->p_offset && head->e_phoff + ((size_t)head->e_phnum * head->e_phentsize) <= ph->p_offset + ph->p_filesz)
            *phdr_addr = ph->p_vaddr + (head->e_phoff - ph->p_offset);


        const uintptr_t align = ph->p_align > pagesize ? ph->p_align : pagesize;
        const uintptr_t end   = ((ph->p_vaddr + ph->p_memsz) & ~(align - 1)) + align;

        if (ph->p_vaddr < space->brk.start)
            space->brk.start = ph->p_vaddr;

        if (end > space->brk.end)
            space->brk.end = end;


        if (arch_vmm_map(space, ph->p_vaddr, -1, ph->p_memsz, ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_TYPE_PAGE) == ARCH_VMM_MAP_FAILED)
            return -ENOMEM;

#if DEBUG_LEVEL_TRACE
        kprintf("sys_execve: PT_LOAD at address(0x%lX) offset(0x%lX) filesz(0x%lX) memsz(0x%lX) alignsize(0x%lX) type(%d)\n", ph->p_vaddr, ph->p_offset, ph->p_filesz, ph->p_memsz, end - ph->p_vaddr, ph->p_type);
#endif

        if (ph->p_filesz) {

            ssize_t e = vfs_read(inode, (void*)ph->p_vaddr, ph->p_offset, ph->p_filesz);

            if (e < 0)
                return -errno;

            if ((size_t)e != ph->p_filesz)
                return -EIO;
        }

        memset((void*)(ph->p_vaddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);


        int flags = ARCH_VMM_MAP_USER | ARCH_VMM_MAP_RDWR;

        if (!(ph->p_flags & PF_X))
            flags |= ARCH_VMM_MAP_NOEXEC;

        if (arch_vmm_mprotect(space, ph->p_vaddr, ph->p_memsz, flags) == ARCH_VMM_MAP_FAILED)
            return -ENOMEM;
    }

    return 0;
}


/**
 * @brief Lays out the strings, pointer vectors, signal stack and user stack of the program being started.
 *
 * @param strings The arguments then the environment, one string after the other.
 * @param size Their total size.
 * @param argc How many are arguments.
 * @param envc How many are environment entries.
 * @param head The ELF header.
 * @param phdr_addr Where the program headers are mapped.
 * @param layout Receives where the stacks went.
 * @return 0 on success, or -ENOMEM.
 */
static long __execve_stack(const char* strings, size_t size, size_t argc, size_t envc, const Elf_Ehdr* head, uintptr_t phdr_addr, struct execve_layout* layout) {

    uintptr_t base   = 0;
    uintptr_t unused = 0;

    if (__execve_sbrk(size, &base) < 0)
        return -ENOMEM;

    if (__execve_sbrk(SIGSTKSZ, &unused) < 0 || __execve_sbrk(0, &layout->sigstack) < 0)
        return -ENOMEM;

    if (__execve_sbrk(sizeof(siginfo_t), &layout->siginfo) < 0)
        return -ENOMEM;

    if (__execve_sbrk(current_task->rlimits[RLIMIT_STACK].rlim_cur, &layout->bottom) < 0 || __execve_sbrk(0, &layout->stack) < 0)
        return -ENOMEM;


    if (size) {

        uio_lock(base, size);
        memcpy((void*)base, strings, size);
        uio_unlock(base, size);
    }


    uintptr_t* sp = (uintptr_t*)layout->bottom;
    size_t off    = 0;

    uio_wptr(sp++, argc);

    for (size_t i = 0; i < argc; i++) {
        uio_wptr(sp++, base + off);
        off += strlen(strings + off) + 1;
    }

    uio_wptr(sp++, 0UL);

    for (size_t i = 0; i < envc; i++) {
        uio_wptr(sp++, base + off);
        off += strlen(strings + off) + 1;
    }

    uio_wptr(sp++, 0UL);


    AUX_ENT(AT_RANDOM, arch_random());
    AUX_ENT(AT_PAGESZ, arch_vmm_getpagesize());
    AUX_ENT(AT_PHDR, phdr_addr);
    AUX_ENT(AT_PHENT, head->e_phentsize);
    AUX_ENT(AT_PHNUM, head->e_phnum);
    AUX_ENT(AT_HWCAP, 0);
    AUX_ENT(AT_HWCAP2, 0);
    AUX_ENT(AT_CLKTCK, arch_timer_generic_getres());
    AUX_ENT(AT_UID, current_task->uid);
    AUX_ENT(AT_GID, current_task->gid);
    AUX_ENT(AT_EUID, current_task->euid);
    AUX_ENT(AT_EGID, current_task->egid);
    AUX_ENT(AT_ENTRY, head->e_entry);
    AUX_ENT(AT_FLAGS, 0);
    AUX_ENT(AT_NULL, 0);

    return 0;
}


/**
 * @brief Puts the caller back on its old address space and frees the one execve() was building.
 *
 * @param old The address space the caller ran on before.
 */
static void __execve_rollback(vmm_address_space_t* old) {

    vmm_address_space_t* built = current_task->address_space;

    scoped_lock(&current_cpu->sched_lock) {
        current_task->address_space = old;
    }

    arch_task_switch_address_space(old);
    arch_vmm_free_address_space(built);
}


/**
 * @brief Records the program name and command line /proc shows, from the new arguments.
 *
 * @param strings The arguments, one string after the other.
 * @param argc How many.
 */
static void __execve_name(const char* strings, size_t argc) {

    const char* base = argc ? strings : "";

    for (const char* q = base; *q; q++) {
        if (*q == '/')
            base = q + 1;
    }

    strncpy(current_task->comm, base, TASK_COMM_LEN - 1);
    current_task->comm[TASK_COMM_LEN - 1] = '\0';


    size_t n      = 0;
    const char* a = strings;

    for (size_t i = 0; i < argc; i++) {

        size_t len = strlen(a) + 1;

        if (n + len > TASK_CMDLINE_LEN)
            break;

        memcpy(&current_task->cmdline[n], a, len);

        n += len;
        a += len;
    }

    current_task->cmdline_len = n;
}



/***
 * Name:        execve
 * Description: execute program
 * URL:         http://man7.org/linux/man-pages/man2/execve.2.html
 *
 * Input Parameters:
 *  0: 0x3B
 *  1: const char  *
 *  2: const char  **
 *  3: const char  **
 *
 * Auto-generated by scripts/gen-syscalls.js
 */


SYSCALL(
    59, execve, long sys_execve(const char* filename, const char** argv, const char** envp) {
        if (unlikely(!filename))
            return -EINVAL;

        if (unlikely(!argv))
            return -EINVAL;

        if (unlikely(!envp))
            return -EINVAL;

        if (unlikely(!uio_check(filename, R_OK)))
            return -EFAULT;

        if (unlikely(!uio_check(argv, R_OK)))
            return -EFAULT;

        if (unlikely(!uio_check(envp, R_OK)))
            return -EFAULT;


#if DEBUG_LEVEL_TRACE
        uio_lock(filename, CONFIG_PATH_MAX);
        {

            kprintf("execve(): filename: '%s'\n", filename);

            for (size_t i = 0; argv[i]; i++)
                kprintf("execve(): argv[%zd]: '%s'\n", i, argv[i]);

            for (size_t i = 0; envp[i]; i++)
                kprintf("execve(): envp[%zd]: '%s'\n", i, envp[i]);
        }
        uio_unlock(filename, CONFIG_PATH_MAX);
#endif



        long e;

        struct stat st = {0};

        scoped_uio_kernel() {
            e = sys_newstat(filename, &st);
        }

        if (unlikely(e < 0))
            return e;

        if (unlikely(!S_ISREG(st.st_mode)))
            return -ENOEXEC;

        if (unlikely(st.st_size == 0))
            return -ENOEXEC;

        if (unlikely(sys_access(filename, X_OK) < 0))
            return -EACCES;



        int fd;
        if ((fd = sys_open(filename, O_RDONLY, 0)) < 0) {
            return fd;
        }


        inode_t* inode __scoped(vfs_inode_cleanup) = NULL;

        shared_ptr_access(current_task->fd, fds, {
            if (fds->descriptors[fd].ref && fds->descriptors[fd].ref->inode)
                inode = vfs_inode_get(fds->descriptors[fd].ref->inode);
        });

        if ((fd = sys_close(fd)) < 0)
            return fd;

        if (unlikely(!inode))
            return -EBADF;



        Elf_Ehdr head = {0};

        if (vfs_read(inode, &head, 0, sizeof(head)) < 0) {
            return -errno;
        }



        if (head.e_ident[0] == '#' && head.e_ident[1] == '!')
            return -ENOEXEC; // TODO: read and execute command scripts


        if ((head.e_ident[EI_MAG0] != ELFMAG0) || (head.e_ident[EI_MAG1] != ELFMAG1) || (head.e_ident[EI_MAG2] != ELFMAG2) || (head.e_ident[EI_MAG3] != ELFMAG3) || (head.e_type != ET_EXEC) || (head.e_entry == 0))
            return -ENOEXEC;

        if (head.e_phentsize != sizeof(Elf_Phdr) || head.e_phnum == 0 || head.e_phnum > EXECVE_PHNUM_MAX)
            return -ENOEXEC;


        const size_t phsize = (size_t)head.e_phnum * sizeof(Elf_Phdr);

        Elf_Phdr* phdrs __scoped(__execve_kfree) = (Elf_Phdr*)kcalloc(head.e_phnum, sizeof(Elf_Phdr), GFP_KERNEL);

        if (unlikely(!phdrs))
            return -ENOMEM;

        if (vfs_read(inode, phdrs, head.e_phoff, phsize) != (ssize_t)phsize)
            return -ENOEXEC;


        vmm_address_space_t* current_space = current_task->address_space;

        if ((e = __execve_check(phdrs, head.e_phnum, current_space->mmap.heap_start)) < 0)
            return e;



        const size_t limit = MAX(current_task->rlimits[RLIMIT_STACK].rlim_cur / 4, EXECVE_ARG_MIN);

        size_t argc = 0;
        size_t envc = 0;
        size_t n    = 0;

        const long argsz = __execve_copy_strings(argv, NULL, limit, &argc);

        if (argsz < 0)
            return argsz;

        const long envsz = __execve_copy_strings(envp, NULL, limit - argsz - argc * sizeof(char*), &envc);

        if (envsz < 0)
            return envsz;


        char* strings __scoped(__execve_kfree) = (char*)kmalloc(argsz + envsz + 1, GFP_KERNEL);

        if (unlikely(!strings))
            return -ENOMEM;

        if ((e = __execve_copy_strings(argv, strings, argsz + argc * sizeof(char*), &n)) != argsz || n != argc)
            return e < 0 ? e : -EFAULT;

        if ((e = __execve_copy_strings(envp, strings + argsz, envsz + envc * sizeof(char*), &n)) != envsz || n != envc)
            return e < 0 ? e : -EFAULT;



        vmm_address_space_t* new_space = arch_vmm_create_address_space(current_space, ARCH_VMM_CLONE_NEW_SPACE);

        new_space->brk.start = ~0UL;
        new_space->brk.end   = 0UL;

        scoped_lock(&current_cpu->sched_lock) {
            current_task->address_space = new_space;
        }

        arch_task_switch_address_space(new_space);


        uintptr_t phdr_addr = 0;

        if ((e = __execve_load(inode, &head, phdrs, new_space, &phdr_addr)) < 0) {
            __execve_rollback(current_space);
            return e;
        }

        new_space->brk.end = (new_space->brk.end & ~(arch_vmm_getpagesize() - 1)) + arch_vmm_getpagesize();

        DEBUG_ASSERT(new_space->brk.start);
        DEBUG_ASSERT(new_space->brk.end);
        DEBUG_ASSERT(new_space->brk.start < new_space->brk.end);


        struct execve_layout layout = {0};

        if ((e = __execve_stack(strings, argsz + envsz, argc, envc, &head, phdr_addr, &layout)) < 0) {
            __execve_rollback(current_space);
            return e;
        }



        sched_group_exit(SIGKILL);

        do_unshare(CLONE_FS);
        do_unshare(CLONE_FILES);
        do_unshare(CLONE_SIGHAND);


        for (size_t i = 0; i < CONFIG_OPEN_MAX; i++) {

            bool cloexec = false;

            shared_ptr_access(current_task->fd, fds, { cloexec = fds->descriptors[i].ref && fds->descriptors[i].close_on_exec; });

            if (cloexec)
                sys_close(i);
        }


        shared_ptr_access(current_task->sighand, sighand, {
            for (size_t i = 0; i < _NSIG; i++) {

                if (sighand->action[i].handler != SIG_IGN)
                    sighand->action[i].handler = SIG_DFL;

                sighand->action[i].sa_flags    = 0;
                sighand->action[i].sa_restorer = NULL;

                memset(sighand->action[i].sa_mask, 0, sizeof(sighand->action[i].sa_mask));
            }
        });


        fs_set_exe(inode);

        __execve_name(strings, argc);


        current_task->userspace.stack    = layout.stack;
        current_task->userspace.sigstack = layout.sigstack;
        current_task->userspace.siginfo  = (siginfo_t*)layout.siginfo;

        arch_vmm_free_address_space(current_space);


        kfree(strings);
        strings = NULL;

        kfree(phdrs);
        phdrs = NULL;

        vfs_inode_put(inode);
        inode = NULL;


        do_vfork_release();


#if DEBUG_LEVEL_TRACE
        kprintf("sys_execve: entering on userspace at address(0x%lX) task(%d) sigstack(0x%lX) stack(0x%lX) bottom(0x%lX) memory(%ld.%ld MB)\n", head.e_entry, current_task->tid, layout.sigstack, layout.stack, layout.bottom,
                (pmm_get_used_memory() / 1024) / 1024, (pmm_get_used_memory() / 1024) % 1024);
#endif


        current_task->flags &= ~TASK_FLAGS_KERNEL_UIO;

        arch_userspace_enter(head.e_entry, layout.stack, (void*)layout.bottom);


        return -EINTR;
    });
