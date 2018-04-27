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


    
    kprintf(LOG "mounts: mount \'root\' in \'%s\' with %s (defaults)\n", rootmnt, rootfs);
    
    if(unlikely(sys_mount(rootmnt, "/root", rootfs, 0, NULL) != E_OK))
        kprintf(ERROR "%s: failed to mount /root\n", rootmnt);

     if(unlikely(sys_mount(NULL, "/root/dev", "devfs", 0, NULL) != E_OK))
        kprintf(ERROR "%s: failed to mount /dev\n", rootmnt);

    if(unlikely(sys_chroot("/root") != E_OK))
        kprintf(ERROR "%s: failed to chroot", "/root");



    FILE* fp = fopen("/etc/fstab", "r");
    if(!fp) {
        kprintf(ERROR "mounts: no /etc/fstab found!\n");
        return E_ERR;
    }
    


    int cl = 1;
    static char buf[BUFSIZ];
    for(; 
        fgets(buf, sizeof(buf), fp) > 0;
        memset(buf, 0, sizeof(buf)), cl++
    ) {
        if(strlen(buf) == 0)
            continue;

        if(buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        if(buf[0] == '#' || buf[0] == '\0')
            continue;
        
        int i = 0;
        char* opt[4];

        char* p;
        for(p = strtok(buf, " "); p && i < 4; p = strtok(NULL, " "))
            opt[i++] = p;

        

        if(i < 4) {
            kprintf(ERROR "/etc/fstab: syntax error at line %d, expected three parameters\n", cl);
            fclose(fp);
            return E_ERR;
        }


        int flags = 0;
        if(strcmp(opt[3], "readonly") == 0)
            flags |= MNT_RDONLY;

        
        if(unlikely(mount(opt[0], opt[1], opt[2], flags, NULL) != 0))
            kprintf(ERROR "%s: failed to mount \'%s\' with \'%s\' (%s)\n", opt[0], opt[1], opt[2], opt[3]);

        kprintf(LOG "mounts: mount \'%s\' in \'%s\' with \'%s\' (%s)\n", opt[0], opt[1], opt[2], opt[3]);
    }
   

    fclose(fp);
    return E_OK;
}