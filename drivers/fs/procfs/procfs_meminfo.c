#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_meminfo_init(procfs_entry_t* e) {
    return 0;
}


int procfs_meminfo_update(procfs_entry_t* e) {
    sprintf(e->data,
        "MemTotal:        %12d kB\n"
        "MemFree:         %12d kB\n"
        "MemAvailable:    %12d kB\n"
        "Buffers:         %12d kB\n"
        "Cached:          %12d kB\n"
        "SwapCached:      %12d kB\n"
        "Active:          %12d kB\n"
        "Inactive:        %12d kB\n"
        "Active(anon):    %12d kB\n"
        "Inactive(anon):  %12d kB\n"
        "Active(file):    %12d kB\n"
        "Inactive(file):  %12d kB\n"
        "Unevictable:     %12d kB\n"
        "Mlocked:         %12d kB\n"
        "SwapTotal:       %12d kB\n"
        "SwapFree:        %12d kB\n"
        "Dirty:           %12d kB\n"
        "Writeback:       %12d kB\n"
        "AnonPages:       %12d kB\n"
        "Mapped:          %12d kB\n"
        "Shmem:           %12d kB\n"
        "Slab:            %12d kB\n"
        "SReclaimable:    %12d kB\n"
        "SUnreclaim:      %12d kB\n"
        "KernelStack:     %12d kB\n"
        "PageTables:      %12d kB\n"
        "NFS_Unstable:    %12d kB\n"
        "Bounce:          %12d kB\n"
        "WritebackTmp:    %12d kB\n"
        "CommitLimit:     %12d kB\n"
        "Committed_AS:    %12d kB\n"
        "VmallocTotal:    %12d kB\n"
        "VmallocUsed:     %12d kB\n"
        "VmallocChunk:    %12d kB\n"
        "HardwareCorrupted: %12d kB\n"
        "AnonHugePages:   %12d kB\n"
        "ShmemHugePages:  %12d kB\n"
        "ShmemPmdMapped:  %12d kB\n"
        "HugePages_Total: %12d\n"
        "HugePages_Free:  %12d\n"
        "HugePages_Rsvd:  %12d\n"
        "HugePages_Surp:  %12d\n"
        "Hugepagesize:    %12d kB\n"
        "DirectMap4k:     %12d kB\n"
        "DirectMap2M:     %12d kB\n"
        "DirectMap1G:     %12d kB\n",

        pmm_state()->total / 1024,
        (pmm_state()->total - pmm_state()->used) / 1024,
        ((pmm_state()->total - pmm_state()->used) + (kvm_state()->total - kvm_state()->used)) / 1024,
        0,
#if CONFIG_CACHE
        kcache_state()->used / 1024,
#else
        0,
#endif
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        kvm_state()->used / 1024,
        0, 0, 
        CONFIG_STACK_SIZE / 1024,
        0, 0, 0, 0, 0, 0, 0, 0,  
        0, 0, 0, 0, 0, 0, 0, 0
    );

    e->size = strlen(e->data);
    return 0;
}