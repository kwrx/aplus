/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <aplus/elf.h>
#include <libc.h>


#define RXX(a, b, c)                                        \
    if(likely(b != -1))                                     \
        sys_lseek(fd, b, SEEK_SET);                         \
    if(unlikely(sys_read(fd, a, c) <= 0)) {                 \
        errno = EIO;                                        \
        return -1;                                          \
    }



extern int elf_check_machine(Elf_Ehdr* elf);
extern char* strndup(const char*, size_t);
extern char** args_dup(char**);



SYSCALL(11, execve,
int sys_execve(const char* filename, char* const argv[], char* const envp[]) {
    if(unlikely(!filename || !argv || !envp)) {
        errno = EINVAL;
        return -1;
    }


    int j = 0;
    for(int i = 0; argv[i]; i++)
        j++;

    for(int i = 0; envp[i]; i++)
        j++;

    if(unlikely(j > TASK_NARGS)) {
        errno = E2BIG;
        return -1;
    }


    int fd = sys_open(filename, O_RDONLY, 0);
    if(fd < 0)
        return -1;


    inode_t* inode = current_task->fd[fd].inode;


    int r;
    if(inode->uid == current_task->uid)
        r = (inode->mode & S_IXUSR);
    else if(inode->gid == current_task->gid)
        r = (inode->mode & S_IXGRP);
    else
        r = (inode->mode & S_IXOTH);


    if(unlikely(!r)) {
        sys_close(fd);

        errno = EACCES;
        return -1;
    }



    
    char sign[2];
    RXX(sign, 0, 2);

    if(strncmp(sign, "#!", 2) == 0) {
        char p[BUFSIZ];
        RXX(p, 2, BUFSIZ);

        for(int i = 0; p[i] && i < BUFSIZ; i++) {
            if(p[i] != '\n')
                continue;

            p[i] = 0;    
            break;
        }

        int argc = 0;
        while(argv[argc])
            argc++;


        char* nargv[argc];
        for(int i = 0; i < argc; i++)
            nargv[i + 1] = argv[i];

        nargv[0] = p;
        return sys_execve(nargv[0], nargv, envp);
    }
    


    Elf_Ehdr hdr;
    RXX(&hdr, 0, sizeof(Elf_Ehdr));

    if ((memcmp(hdr.e_ident, ELFMAG, sizeof(ELFMAG) - 1))       ||
        (elf_check_machine(&hdr))                               ||
        (hdr.e_type != ET_EXEC)) {

        errno = ENOEXEC;
        return -1;
    }




    char** __new_argv = args_dup((char**) argv);
    char** __new_envp = args_dup((char**) envp);


    INTR_OFF;
    task_release(current_task);

    
    size_t size = 0;
    uintptr_t nbase = 0xFFFFFFFF;
    uintptr_t nend = 0;
    int is_dynamic = 0;


    for(int i = 0; i < hdr.e_phnum; i++) {
        Elf_Phdr phdr;
        RXX(&phdr, hdr.e_phoff + (i * hdr.e_phentsize), hdr.e_phentsize);

        switch(phdr.p_type) {
            case PT_LOAD:

                size = (phdr.p_memsz + phdr.p_align - 1) & ~(phdr.p_align - 1);

                if(phdr.p_vaddr < nbase)
                    nbase = phdr.p_vaddr;

                if(size + phdr.p_vaddr > nend)
                    nend = size + phdr.p_vaddr;
                
                
                if(unlikely(!sys_mmap((void*) phdr.p_vaddr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_ANON, -1, 0))) {            
                    kprintf(ERROR "elf: invalid mapping 0x%x (%d Bytes)\n", phdr.p_vaddr, size);
                    
                    errno = EFAULT;
                    return -1;
                }

                RXX((void*) phdr.p_vaddr, phdr.p_offset, phdr.p_filesz);
                break;

            case PT_DYNAMIC:
                is_dynamic++;
                break;

            default:
                continue;
        }
    }

    KASSERT(nbase != 0xFFFFFFFF);
    size = nend - nbase;

    if(is_dynamic)
        kprintf(WARN "elf: dynamic object not yet supported!\n");
    
    sys_close(fd);



    void (*__start) (char**, char**) = (void (*) (char**, char**)) hdr.e_entry;
    KASSERT(__start);


    kprintf(LOG "execve: loading \'%s\' at %p:%p (%d Mb)\n", __new_argv[0], __start, V2P(__start), pmm_state()->used / 1024 / 1024);


    current_task->argv = __new_argv;
    current_task->environ = __new_envp;
    current_task->image->start = (uintptr_t) __start;
    current_task->image->end = ((current_task->image->start + size) & ~(PAGE_SIZE - 1)) + 0x10000;
    current_task->image->refcount = 1;
    current_task->vmsize += current_task->image->end - current_task->image->start;
    current_task->name = strdup(basename(__new_argv[0]));
    current_task->description = "";
    current_task->exe = inode;

    list_clear(current_task->signal.s_queue);



    INTR_ON;
    syscall_ack();


    __start(__new_argv, __new_envp);
    __builtin_unreachable();
});
