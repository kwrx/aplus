#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>

#include <sys/ioctl.h>
#include <aplus/fbdev.h>

#define SYSCONFIG       "/etc/config"
#define APPS_GUI        "/usr/bin/gnx2"
#define APPS_SHELL      "/usr/bin/sh"

#define GRAPHICS_WIDTH      800
#define GRAPHICS_HEIGHT     600
#define GRAPHICS_BPP        32



extern char* ini_read(FILE* fp, const char* ini_name);
extern int ini_read_int_or(FILE* fp, const char* ini_name, int onerr);


int main(int argc, char** argv) {
    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);
    
    char* __envp[] = {
        "BASH=" APPS_SHELL,
        "GUI=" APPS_GUI,
        "HOME=/home",
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin",
        NULL
    };
    
    char* __argv[] = {
        APPS_SHELL,
        "--verbose",
        NULL
    };
    
    
    
    static int graphics_enabled = -1;
    static struct option long_options[] = {
        { "graphics", no_argument, &graphics_enabled, 1},
        { "console", no_argument, &graphics_enabled, 0}
    };
    
    int i, idx;
    while((i = getopt_long(argc, argv, "gc", long_options, &idx)) != -1) {
        switch(i) {
            case 0:
                break;
            case 'g':
                graphics_enabled = 1;
                break;
            case 'c':
                graphics_enabled = 0;
                break;
            default:
                fprintf(stderr, "init: invalid argument %d\n", i);
                abort();
        }     
    }
    
    
    FILE* fp = fopen(SYSCONFIG, "r");
    if(!fp) {
        fprintf(stderr, SYSCONFIG ": not found! Using default settings\n");
        return 0;
    }

    int v;
    if(v = ini_read_int_or(fp, "screen.enabled", 0) && abs(graphics_enabled) == 1) {
        int fd = open("/dev/fb0", O_RDONLY);
        if(fd < 0)
            perror("/dev/fb0");
        else {
            
            fbdev_mode_t mode;
            mode.width = ini_read_int_or(fp, "screen.width", GRAPHICS_WIDTH);
            mode.height = ini_read_int_or(fp, "screen.height", GRAPHICS_HEIGHT);
            mode.bpp = ini_read_int_or(fp, "screen.bpp", GRAPHICS_BPP);
            mode.vx =
            mode.vy = 0;
            
            ioctl(fd, FBIOCTL_SETMODE, &mode);
            ioctl(fd, FBIOCTL_ENABLE, 1);
            close(fd);
            
            __argv[0] = APPS_GUI;
        }
    }
    
    int p;
    if(p = ini_read_int_or(fp, "idle.priority", 0)) {
        /* TODO */
    }
    
    fclose(fp);    
    return execve(__argv[0], __argv, __envp);
}