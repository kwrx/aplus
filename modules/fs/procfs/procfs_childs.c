#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <libc.h>

#include "procfs.h"


static int environ_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);
    
    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    if(unlikely(!tk->environ))
        return E_OK;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    int i;
    for(i = 0; tk->environ[i]; i++) {
        strcat(buf, tk->environ[i]);
        strcat(buf, "\0");
    }

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}

static int cmdline_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);

    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    if(unlikely(!tk->argv))
        return E_OK;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    int i;
    for(i = 0; tk->argv[i]; i++) {
        strcat(buf, tk->argv[i]);
        strcat(buf, "\0");
    }

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}



static int status_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);

    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, 
        "Name:        %s\n"
        "Description: %s\n"
        "Umask:       %d\n"
        "State:       %s\n"
        "Pid:         %d\n"
        "PPid:        %d\n"
        "Uid:         %d\n"
        "Gid:         %d\n"
        "Sid:         %d\n"
        "FDSize:      %d\n"
        "Priority:    %d\n"
        "VmSize:      %08d kB\n"
        "VmStk:       %08d kB\n"
        "VmExe:       %08d kB\n",
        tk->name,
        tk->description ? tk->description : "",
        tk->umask,
        (
            (tk->status == TASK_STATUS_READY ? "S (Sleeping)" :
            (tk->status == TASK_STATUS_SLEEP ? "S (Sleeping)" :
            (tk->status == TASK_STATUS_RUNNING ? "R (Running)" :
            (tk->status == TASK_STATUS_KILLED ? "X (Dead)" : "Unknown"))))
        ),
        tk->pid,
        tk->parent ? tk->parent->pid : 0,
        tk->uid,
        tk->gid,
        tk->sid,
        TASK_FD_COUNT,
        tk->priority,
        tk->vmsize / 1024,
        CONFIG_STACK_SIZE / 1024,
        tk->exe ? (int) tk->exe->size / 1024 : 0
    );

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}

static int stat_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);

    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, 
        "%d (%s) %s %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld "
        "%ld %ld %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u "
        "%u %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %d\n",
        tk->pid,
        tk->name,
        (
            (tk->status == TASK_STATUS_READY ? "S" :
            (tk->status == TASK_STATUS_SLEEP ? "S" :
            (tk->status == TASK_STATUS_RUNNING ? "R" :
            (tk->status == TASK_STATUS_KILLED ? "X" : "N"))))
        ),
        tk->parent ? tk->parent->pid : 0,
        tk->gid,
        tk->sid,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        tk->clock.tms_utime,
        tk->clock.tms_stime,
        tk->clock.tms_cutime,
        tk->clock.tms_cstime,
        tk->priority,
        tk->priority,
        1,
        0,
        0,
        tk->vmsize,
        tk->vmsize / PAGE_SIZE,
        -1,
        tk->image->start,
        tk->image->end,
        CONFIG_STACK_BASE,
        tk->context,
        tk->context,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        tk->priority,
        0,
        0,
        0,
        0,
        0,
        0,
        tk->image->end,
        tk->argv,
        tk->argv,
        tk->environ,
        tk->environ,
        tk->exit.value
    );

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}


static int statm_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);

    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, 
        "%u %u %u %u %u %u %u\n",
        (tk->vmsize),
        (tk->image->end - tk->image->start),
        0,
        0,
        0,
        CONFIG_STACK_SIZE,
        0
    );

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}


static int io_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    if(pfs->data)
        kfree(pfs->data);

    volatile task_t* tk = pfs->tk;
    if(tk == NULL)
        tk = current_task;

    static char buf[8192];
    memset(buf, 0, sizeof(buf));

    sprintf(buf, 
        "rchar:                 %lu\n"
        "wchar:                 %lu\n"
        "syscr:                 %lu\n"
        "syscw:                 %lu\n"
        "read_bytes:            %lu\n"
        "write_bytes:           %lu\n"
        "cancelled_write_bytes: %lu\n",
        (unsigned long) tk->iostat.rchar,
        (unsigned long) tk->iostat.wchar,
        (unsigned long) tk->iostat.syscr,
        (unsigned long) tk->iostat.syscw,
        (unsigned long) tk->iostat.read_bytes,
        (unsigned long) tk->iostat.write_bytes,
        (unsigned long) tk->iostat.cancelled_write_bytes
    );

    pfs->data = strdup(buf);
    inode->size = (off64_t) strlen(buf);

    return E_OK;
}


static int procfs_cwd_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    volatile task_t* tk = pfs->tk;

    if(tk == NULL)
        tk = current_task;

    inode->link = tk->cwd;
    return E_OK;
}

static int procfs_root_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    volatile task_t* tk = pfs->tk;

    if(tk == NULL)
        tk = current_task;

    inode->link = tk->root;
    return E_OK;
}

static int procfs_exe_open(inode_t* inode) {
    if(unlikely(!inode))
        return E_ERR;

    if(unlikely(!inode->userdata))
        return E_ERR;

    procfs_t* pfs = (procfs_t*) inode->userdata;
    volatile task_t* tk = pfs->tk;

    if(tk == NULL)
        tk = current_task;

    inode->link = tk->exe;
    return E_OK;
}


int procfs_add_childs(inode_t* parent, task_t* tk) {
    #define mknod_or_error(x)                                       \
        inode_t* x = vfs_mknod(parent, #x, S_IFREG | 0666);         \
        if(unlikely(!x)) {                                          \
            kprintf(ERROR "procfs: error on creating %s\n", #x);    \
        }


    mknod_or_error(cwd);
    mknod_or_error(root);
    mknod_or_error(exe);
    mknod_or_error(environ);
    mknod_or_error(cmdline);
    mknod_or_error(status);
    mknod_or_error(stat);
    mknod_or_error(statm);
    mknod_or_error(io);


    cwd->userdata = procfs_make_userdata(tk, NULL);
    cwd->open = procfs_cwd_open;
    cwd->mode = (cwd->mode & ~S_IFREG) | S_IFLNK;

    root->userdata = procfs_make_userdata(tk, NULL);
    root->open = procfs_root_open;
    root->mode = (root->mode & ~S_IFREG) | S_IFLNK;

    exe->userdata = procfs_make_userdata(tk, NULL);
    exe->open = procfs_exe_open;
    exe->mode = (exe->mode & ~S_IFREG) | S_IFLNK;


    environ->userdata = procfs_make_userdata(tk, NULL);
    environ->open = environ_open;
    environ->read = procfs_read;

    cmdline->userdata = procfs_make_userdata(tk, NULL);
    cmdline->open = cmdline_open;
    cmdline->read = procfs_read;

    status->userdata = procfs_make_userdata(tk, NULL);
    status->open = status_open;
    status->read = procfs_read;

    stat->userdata = procfs_make_userdata(tk, NULL);
    stat->open = stat_open;
    stat->read = procfs_read;

    statm->userdata = procfs_make_userdata(tk, NULL);
    statm->open = statm_open;
    statm->read = procfs_read;

    io->userdata = procfs_make_userdata(tk, NULL);
    io->open = io_open;
    io->read = procfs_read;
    

    return 0;
}