#include <xdev.h>
#include <xdev/debug.h>
#include <libc.h>

int mounts_init(void) {
    #define mount_and_check(a, b, c)                                \
        {                                                           \
            if(unlikely(sys_mount(a, b, c, 0, NULL) != E_OK))       \
                kprintf(ERROR, "%s: failed to mount %s\n", a, b);   \
        }
    
    #define relink(a)                                               \
        {                                                           \
            if(unlikely(sys_symlink(a, CONFIG_ROOT a) != E_OK))     \
                kprintf(ERROR, "%s: failed to link in %s",          \
                    a, CONFIG_ROOT a);                              \
        }
    
    
	mount_and_check("/dev/cd0", "/cdrom", "iso9660");
    relink("/dev");

    
    if(unlikely(sys_chroot(CONFIG_ROOT) != 0))
        kprintf(ERROR, "%s: failed to chroot", CONFIG_ROOT);
        
    mount_and_check(NULL, "/tmp", "tmpfs");
    mount_and_check(NULL, "/proc", "procfs");
        
    return E_OK;
}