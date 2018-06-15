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


#define _WITH_MNTFLAGS 1
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <libc.h>



int mounts_init(void) {
    #define mount_and_check(a, b, c)                                \
        {                                                           \
            if(unlikely(sys_mount(a, b, c, 0, NULL) != 0))          \
                kprintf(ERROR "%s: failed to mount %s\n", a, b);    \
        }
    
    #define relink(a)                                               \
        {                                                           \
            if(unlikely(sys_symlink(a, CONFIG_ROOT a) != 0))        \
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
    
    if(unlikely(sys_mount(rootmnt, "/root", rootfs, 0, NULL) != 0))
        kprintf(ERROR "%s: failed to mount /root: %s\n", rootmnt, strerror(errno));

     if(unlikely(sys_mount(NULL, "/root/dev", "devtmpfs", MS_NODEV | MS_NOSUID | MS_KERNMOUNT, NULL) != 0))
        kprintf(ERROR "%s: failed to mount /dev: %s\n", rootmnt, strerror(errno));

    if(unlikely(sys_chroot("/root") != 0))
        kprintf(ERROR "%s: failed to chroot: %s", "/root", strerror(errno));



    FILE* fp = fopen("/etc/fstab", "r");
    if(!fp) {
        kprintf(ERROR "mounts: no /etc/fstab found!\n");
        return -1;
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
            return -1;
        }


        int flags = 0;

        for(p = strtok(opt[3], ","); p; p = strtok(NULL, ",")) {
            for(i = 0; mnt_flags[i].option; i++) {
                if(strcmp(p, mnt_flags[i].option) != 0)
                    continue;
            
                flags |= mnt_flags[i].value;
                break;
            }
        }

        
        if(unlikely(sys_mount(opt[0], opt[1], opt[2], flags, NULL) != 0))
            kprintf(ERROR "%s: failed to mount \'%s\' with \'%s\' (%s): %s\n", opt[0], opt[1], opt[2], opt[3], strerror(errno));

        kprintf(LOG "mounts: mount \'%s\' in \'%s\' with \'%s\' (%s)\n", opt[0], opt[1], opt[2], opt[3]);
    }
   

    fclose(fp);
    return 0;
}