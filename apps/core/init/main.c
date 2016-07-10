#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <aplus/fbdev.h>

#define SYSCONFIG       "/etc/config"



extern int ini_read(FILE* fp, const char* ini_name);

int main() {
    open("/dev/stdin", O_RDONLY);
    open("/dev/stdout", O_WRONLY);
    open("/dev/stderr", O_WRONLY);
    
    FILE* fp = fopen(SYSCONFIG, "r");
    if(!fp) {
        fprintf(stderr, SYSCONFIG ": not found! Using default settings\n");
        return 0;
    }


    if(ini_read(fp, "screen.enabled")) {
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
        }
        
        close(fd);
    }
    
    int p;
    if(p = ini_read(fp, "idle.priority")) {
        /* TODO */
    }
    
    fclose(fp);
    
    
    
    char* __envp[] = {
        "BASH=/usr/bin/sh",
        "HOME=/home",
        "PATH=/usr/bin:/usr/local/bin",
        NULL
    };
    
    char* __argv[] = {
        "/usr/bin/lua",
        NULL
    };
    
    return execve(__argv[0], __argv, __envp);
}