#include <aplus.h>
#include <aplus/debug.h>
#include <libc.h>

int mounts_init(void) {
    #define mount_and_check(a, b, c)                                \
        {                                                           \
            if(unlikely(sys_mount(a, b, c, 0, NULL) != E_OK))       \
                kprintf(ERROR "%s: failed to mount %s\n", a, b);   \
        }
    
    #define relink(a)                                               \
        {                                                           \
            if(unlikely(sys_symlink(a, CONFIG_ROOT a) != E_OK))     \
                kprintf(ERROR "%s: failed to link in %s",          \
                    a, CONFIG_ROOT a);                              \
        }
    
#if DEBUG
    kprintf(LOG "mounts: mount root in %s\n", CONFIG_ROOT);
#endif    
    
    if(unlikely(sys_mount(CONFIG_ROOT, "/root", CONFIG_ROOTFS, 0, NULL) != E_OK))
        kprintf(ERROR "/dev/hd0/0: failed to mount /root\n");

    if(unlikely(sys_symlink("/dev", "root/dev") != E_OK))
        kprintf(ERROR "/dev: failed to link in /root/dev");

    if(unlikely(sys_chroot("/root") != E_OK))
        kprintf(ERROR "%s: failed to chroot", "/root");


    return E_OK;
}