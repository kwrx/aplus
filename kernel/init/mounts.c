#include <aplus.h>
#include <aplus/debug.h>
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
    
    
    if(unlikely(sys_mount("/dev/cd0", "/cdrom", "iso9660", 0, NULL) != E_OK))
        kprintf(ERROR, "/dev/cd0: failed to mount /cdrom\n");

    if(unlikely(sys_symlink("/dev", CONFIG_ROOT "/dev") != E_OK))
        kprintf(ERROR, "/dev: failed to link in " CONFIG_ROOT "/dev");

    if(unlikely(sys_chroot(CONFIG_ROOT) != 0))
        kprintf(ERROR, "%s: failed to chroot", CONFIG_ROOT);


    
    
    int fd = open("/etc/fstab", O_RDONLY);
    if(fd < 0)
        return E_OK;

    
    int cl = 1;

    char ch;
    while(read(fd, &ch, 1) == 1) {
        static char ln[32 * 3];
        memset(ln, 0, sizeof(ln));

        int cm = 0;
        int tk = -1;
        int i = 0;
        int p = 0;


        do {
            if(ch == '#')
                cm = 1;

            if(ch == '\n' || ch == '\r')
                break;

            if(cm)
                continue;

            if(ch == ' ') {
                if(tk < 1)
                    continue;
                
                tk = 0;
                p = 0;
                i++;
                continue;
            }

            ln[32 * i + p++] = ch;
            ln[32 * i + p] = 0;

            tk = 1;
        } while(read(fd, &ch, 1) == 1);

        if(tk == -1)
            continue;

        if(i < 2) {
            kprintf(ERROR, "/etc/fstab: syntax error at line %d\n", cl);
            continue;
        }


        #define __p(x)  \
            (strcmp(&ln[x * 32], "0") == 0 ? NULL : &ln[x * 32])
        
        if(unlikely(sys_mount(__p(0), __p(1), __p(2), 0, NULL) != E_OK))
            kprintf(ERROR, "%s: failed to mount \'%s\' with \'%s\'\n", __p(0), __p(1), __p(2));


        kprintf(INFO, "/etc/fstab: mount \'%s\' in \'%s\' with \'%s\'\n", __p(0), __p(1), __p(2));
        cl++;
    }

    close(fd);   
        
    return E_OK;
}