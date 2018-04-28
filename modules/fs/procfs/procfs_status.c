#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_status_init(procfs_entry_t* e) {
    return 0;
}


int procfs_status_update(procfs_entry_t* e) {
    volatile task_t* tk = e->task
                            ? e->task : current_task;

    sprintf(e->data,
        "Name:          %s\n"
        "Umask:         0%o\n"
        "State:         %s\n"
        "Tgid:          %d\n"
        "Ngid:          %d\n"
        "Pid:           %d\n"
        "PPid:          %d\n"
        "TracerPid:     %d\n"
        "Uid:           %4d %4d %4d %4d\n"
        "Gid:           %4d %4d %4d %4d\n"
        "FDSize:        %d\n"
        "Groups:        %d\n"
        "NStgid:        %d\n"
        "NSpid:         %d\n"
        "NSpgid:        %d\n"
        "NSsid:         %d\n"
        "VmPeak:        %12d kB\n"
        "VmSize:        %12d kB\n"
        "VmLck:         %12d kB\n"
        "VmPin:         %12d kB\n"
        "VmHWM:         %12d kB\n"
        "VmRSS:         %12d kB\n"
        "RssAnon:       %12d kB\n"
        "RssFile:       %12d kB\n"
        "RssShmem:      %12d kB\n"
        "VmData:        %12d kB\n"
        "VmStk:         %12d kB\n"
        "VmExe:         %12d kB\n"
        "VmLib:         %12d kB\n"
        "VmPTE:         %12d kB\n"
        "VmPMD:         %12d kB\n"
        "VmSwap:        %12d kB\n"
        "HugetlbPages:  %12d kB\n"
        "Threads:       %d\n"
        "SigQ:          0/0\n"
        "SigPnd:        0000000000000000\n"
        "ShdPnd:        0000000000000000\n"
        "SigBlk:        0000000000000000\n"
        "SigIgn:        0000000000000000\n"
        "SigCgt:        0000000000000000\n"
        "CapInh:        0000000000000000\n"
        "CapPrm:        0000000000000000\n"
        "CapEff:        0000000000000000\n"
        "CapBnd:        ffffffffffffffff\n"
        "CapAmb:        0000000000000000\n"
        "NoNewPrivs:    0\n"
        "Seccomp:       0\n"
        "Cpus_allowed:  00000001\n"
        "Cpus_allowed_list:    0\n"
        "Mems_allowed:         1\n"
        "Mems_allowed_list:    0\n"
        "voluntary_ctxt_switches:        0\n"
        "nonvoluntary_ctxt_switches:     0\n",

        tk->name,
        tk->umask,
        (
            (tk->status == TASK_STATUS_READY   ? "S (sleeping)"   :
            (tk->status == TASK_STATUS_SLEEP   ? "S (sleeping)"   :
            (tk->status == TASK_STATUS_RUNNING ? "R (running)"    :
            (tk->status == TASK_STATUS_KILLED  ? "Z (zombie)" : "X (dead)"))))
        ),
        tk->tgid,
        0,
        tk->pid,
        tk->parent ? tk->parent->pid : 0,
        0,
        tk->uid, tk->uid, tk->uid, tk->uid,
        tk->gid, tk->gid, tk->gid, tk->gid,
        TASK_FD_COUNT,
        0,
        tk->tgid,
        tk->pid,
        tk->pgid,
        tk->sid,
        0,
        tk->vmsize / 1024,
        0,
        0,
        0,
        (tk->image->end - tk->image->start) / 1024,
        (tk->image->end - tk->image->start) / 1024,
        0,
        0,
        0,
        CONFIG_STACK_SIZE / 1024,
        tk->exe ? tk->exe->size : 0,
        0,
        0,
        0,
        0,
        0,
        1
    );

    e->size = strlen(e->data);
    return 0;
}