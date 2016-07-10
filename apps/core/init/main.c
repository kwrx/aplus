#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <aplus/fbdev.h>

#define SYSCONFIG       "/etc/config"
#define APPS_GUI        "/usr/bin/gnx"
#define APPS_SHELL      "/usr/bin/sh"



extern int ini_read(FILE* fp, const char* ini_name);

int main() {
    open("/dev/stdin", O_RDONLY);
    open("/dev/stdout", O_WRONLY);
    open("/dev/stderr", O_WRONLY);
    
    char* __envp[] = {
        "BASH=" APPS_SHELL,
        "HOME=/home",
        "PATH=/usr/bin:/usr/local/bin",
        NULL
    };
    
    char* __argv[] = {
        APPS_SHELL,
        NULL,
    };
    
    
    
    
    FILE* fp = fopen(SYSCONFIG, "r");
    if(!fp) {
        fprintf(stderr, SYSCONFIG ": not found! Using default settings\n");
        return 0;
    }

    int v;
    if(v = ini_read(fp, "screen.enabled")) {
        int fd = open("/dev/fb0", O_RDONLY);
        if(fd < 0)
            perror("/dev/fb0");
        else {
            
            fbdev_mode_t mode;
            mode.width = ini_read(fp, "screen.width");
            mode.height = ini_read(fp, "screen.height");
            mode.bpp = ini_read(fp, "screen.bpp");
            mode.vx =
            mode.vy = 0;
            
            ioctl(fd, FBIOCTL_SETMODE, &mode);
            ioctl(fd, FBIOCTL_ENABLE, 1);
            close(fd);
            
            __argv[0] = APPS_GUI;
        }
    }
    
    int p;
    if(p = ini_read(fp, "idle.priority")) {
        /* TODO */
    }
    
    fclose(fp);
    return execve(__argv[0], __argv, __envp);
}