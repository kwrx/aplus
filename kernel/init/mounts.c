#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <libc.h>

int mounts_init(void) {
    #define mount_and_check(a, b, c)                                \
        {                                                           \
            if(unlikely(sys_mount(a, b, c, 0, NULL) != E_OK))       \
                kprintf(ERROR "%s: failed to mount %s\n", a, b);    \
        }
    
    #define relink(a)                                               \
        {                                                           \
            if(unlikely(sys_symlink(a, CONFIG_ROOT a) != E_OK))     \
                kprintf(ERROR "%s: failed to link in %s",           \
                    a, CONFIG_ROOT a);                              \
        }


    char rootmnt[32] = { 0 };
    char rootfs[32] = { 0 };
    char* cmdline = strdup(mbd->cmdline.args);

    char* p;
    for(p = strtok(cmdline, " "); p; p = strtok(NULL, " ")) {
        if(strncmp(p, "root=", 5) == 0)
            strcpy(rootmnt, &p[5]);
        
        else if(strncmp(p, "rootfs=", 7) == 0)
            strcpy(rootfs, &p[7]);
    }

    kfree(cmdline);


    
    kprintf(LOG "mounts: mount root in %s (%s)\n", rootmnt, rootfs);
    
    if(unlikely(sys_mount(rootmnt, "/root", rootfs, 0, NULL) != E_OK))
        kprintf(ERROR "/dev/hd0/0: failed to mount /root\n");

    if(unlikely(sys_symlink("/dev", "root/dev") != E_OK))
        kprintf(ERROR "/dev: failed to link in /root/dev");

    if(unlikely(sys_chroot("/root") != E_OK))
        kprintf(ERROR "%s: failed to chroot", "/root");


    return E_OK;
}