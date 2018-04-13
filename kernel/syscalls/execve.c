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


extern int elf_check_machine(Elf_Ehdr* elf);
extern char* strndup(const char*, size_t);
extern char** args_dup(char**);



SYSCALL(2, execve,
int sys_execve(const char* filename, char* const argv[], char* const envp[]) {

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


    
    #define RXX(a, b, c)                                        \
        if(likely(b != -1))                                     \
            sys_lseek(fd, b, SEEK_SET);                         \
        if(unlikely(sys_read(fd, a, c) <= 0)) {                 \
            errno = EIO;                                        \
            return -1;                                          \
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


        char* argv[32];
        memset(argv, 0, sizeof(argv));

        int i = 0;
        for(char* s = strtok((char*) strdup(p), " "); s; s = strtok(NULL, " "))
            argv[i++] = s;
        argv[i++] = NULL;



        if(sys_lseek(fd, strlen(p) + 2, SEEK_SET) != -1) {
            int fp = sys_open(tmpnam(NULL), O_CREAT | O_RDWR, S_IFREG | 0666);
            if(fp < 0)
                return -1;
            

            sys_fcntl(fp, F_DUPFD, STDIN_FILENO);
            
            size_t size;
            while((size = sys_read(fd, p, BUFSIZ)))
                sys_write(STDIN_FILENO, p, size);

            sys_lseek(STDIN_FILENO, 0, SEEK_SET);
            sys_close(fd);
        }

        return sys_execve(argv[0], argv, current_task->environ);
    }
    


    Elf_Ehdr hdr;
    RXX(&hdr, 0, sizeof(Elf_Ehdr));

    if ((memcmp(hdr.e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1))         ||
        (elf_check_machine(&hdr))                                       ||
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



    void (*_start) (char**, char**) = (void (*) (char**, char**)) hdr.e_entry;
    KASSERT(_start);



    current_task->argv = __new_argv;
    current_task->environ = __new_envp;
    current_task->image->start = (uintptr_t) _start;
    current_task->image->end = ((current_task->image->start + size + PAGE_SIZE) & ~(PAGE_SIZE - 1)) + 0x10000;
    current_task->vmsize += current_task->image->end - current_task->image->start;
    current_task->name = __new_argv[0];
    current_task->description = "";
    current_task->exe = inode;


    INTR_ON;
    syscall_ack();

    _start((char**) current_task->argv, (char**) current_task->environ);
    KASSERT(0);
    
    return -1;
});
