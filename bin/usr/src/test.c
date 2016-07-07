#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/fbdev.h>
//#include <sys/ioctl.h>

int main(int argc, char** argv, char** environ) {
    int fd = open("/dev/fb0", O_RDONLY);
    if(fd < 0)
        perror("/dev/fb0");
        
    fbdev_mode_t fb;
    ioctl(fd, FBIOCTL_ENABLE, NULL);
    ioctl(fd, FBIOCTL_GETMODE, &fb);
    
   
    memset(fb.lfbptr, 0xAA, 1280 * 768 * 4);
    
    
    printf("Hello World\n");
  
    return 0;
}