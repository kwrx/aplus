/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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

#define _GNU_SOURCE
#include <sched.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <termios.h>

#include <aplus/fb.h>

#include <pthread.h>


#define __trim(str)         \
    while(isblank(*str))    \
        str++




static void init_framebuffer(void) {

    int fd = open("/dev/fb0", O_RDONLY);
    
    if(fd < 0)
        return (void) fprintf(stderr, "fb0: no framebuffer device found\n");

    
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    ioctl(fd, FBIOGET_VSCREENINFO, &var);
    ioctl(fd, FBIOPUT_VSCREENINFO, &var);
    ioctl(fd, FBIOGET_FSCREENINFO, &fix);
    
    close(fd);


#if defined(DEBUG)
    fprintf(stderr, "fb0: initialized framebuffer device %dx%dx%d [ptr(%p), size(%p)]\n", 
        var.xres, 
        var.yres, 
        var.bits_per_pixel, 
        (void*) ((uintptr_t) fix.smem_start), 
        (void*) ((uintptr_t) fix.smem_len)
    );
#endif

#if defined(DEBUG)
    //memset((void*) fix.smem_start, 0xFF, var.xres * var.yres * (var.bits_per_pixel >> 3));
    uint32_t* p = (uint32_t*) ((uintptr_t) fix.smem_start);
    for(int i = 0; i < var.xres * var.yres * (var.bits_per_pixel >> 3) - 1000; i += sizeof(uint32_t))
        p[i] = 0x00FFFFFF;
#endif

}


static void init_welcome(void) {

    FILE* fp = fopen("/etc/motd", "r");

    if(!fp)
        return;


    char line[BUFSIZ];
    while(fgets(line, sizeof(line), fp) > 0)
        fprintf(stdout, line);


    fflush(stdout);
    fclose(fp);

}


static void init_environment(void) {

    FILE* fp = fopen("/etc/environment", "r");

    if(!fp)
        return (void) fprintf(stderr, "env: no environment file found\n");


    char line[BUFSIZ];
    while(fgets(line, sizeof(line), fp) > 0) {

        if(line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';


        char* p = &line[0];
        __trim(p);

        
        switch(*p) {

            case '\0':
            case  '#':
                continue;

            default:
            
                if(strchr(p, '='))
                    putenv(p);

                break;

        }

    }


    fclose(fp);

}


static void init_fstab() {

    FILE* fp = fopen("/etc/fstab", "r");

    if(!fp)
        return (void) fprintf(stderr, "fstab: no fstab config file found");


    char line[BUFSIZ];
    while(fgets(line, sizeof(line), fp) > 0) {

        if(line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';


        char* p = &line[0];
        __trim(p);

        
        switch(*p) {

            case '\0':
            case  '#':
                continue;

            default:
                {

                    int flags = 0;

                    char dev[128];
                    char dir[128];
                    char fs [32];
                    char fl [128];

                    if(sscanf(p, "%s %s %s %s", dev, dir, fs, fl) <= 0)
                        break;

#if defined(DEBUG)
                    fprintf(stderr, "fstab: mount device %s in %s [fstype(%s), flags(%s)]\n", dev, dir, fs, fl);
#endif

                    
                    for(char* k = strtok(fl, ","); k; k = strtok(NULL, ",")) {

                        #define has(str, flag)          \
                            if(strcmp(k, str) == 0) {   \
                                flags |= flag;          \
                                continue;               \
                            }
                        

                        has("defaults",     MS_NOATIME | MS_SHARED | MS_NOSUID)
                        has("noatime",      MS_NOATIME)
                        has("nodev",        MS_NODEV)
                        has("nodiratime",   MS_NODIRATIME)
                        has("dirsync",      MS_DIRSYNC)
                        has("noexec",       MS_NOEXEC)
                        has("iversion",     MS_I_VERSION)
                        has("mand",         MS_MANDLOCK)
                        has("relatime",     MS_RELATIME)
                        has("lazytime",     MS_LAZYTIME)
                        has("nosuid",       MS_NOSUID)
                        has("remount",      MS_REMOUNT)
                        has("ro",           MS_RDONLY)
                        has("sync",         MS_SYNCHRONOUS)
                        has("nouser",       MS_NOUSER)
                        has("silent",       MS_SILENT)


                    }


                    if(mount(dev, dir, fs, flags, NULL) < 0)
                        perror("mount()");

                    
                }
                break;

        }

    }

    fclose(fp);
    
}


static void init_hostname() {

    FILE* fp = fopen("/etc/hostname", "r");

    if(!fp)
        return;


    char hostname[BUFSIZ] = { 0 };

    fgets(hostname, BUFSIZ, fp);
    fclose(fp);


  
    if(strlen(hostname) > 0) {

        if(hostname[strlen(hostname) - 1] == '\n')
            hostname[strlen(hostname) - 1] = '\0';

        syscall(SYS_sethostname, hostname, strlen(hostname));

    }



#if defined(DEBUG)
    fprintf(stderr, "hostname: set hostname '%s'\n", hostname);
#endif

}


static void init_initd(void) {

    cpu_set_t cpus;
    CPU_ZERO(&cpus);

    for(int cpu = 0; cpu < (CPU_SETSIZE << 3); cpu++)
        CPU_SET(cpu, &cpus);

    sched_setaffinity(0, sizeof(cpu_set_t), &cpus);


    // TODO: run /etc/init.d scripts...
}



int main(int argc, char** argv, char** envp) {

    int fd = open("/dev/kmsg", O_RDWR);

    if(fd < 0)
        return 1;

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);


#if defined(DEBUG)
    if(getpid() != 1)
        fprintf(stderr, "init: WARN! getpid() != 1\n");
#endif


    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);


    setsid();
    tcsetpgrp(STDIN_FILENO, getpgrp());


    init_environment();
    init_framebuffer();
    init_fstab();
    init_hostname();
    init_welcome();
    init_initd();

    setpriority(0, PRIO_PROCESS, 19);

    

    fprintf(stderr, "init: going to sleep...\n");
    
    do {
        waitpid(-1, NULL, 0);
    } while(errno != ECHILD);
        

    fprintf(stderr, "init: unreachable point! system halted!\n");

    for(;;)
        ;

}