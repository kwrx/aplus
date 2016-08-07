#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <aplus/gnx.h>


char* gnx_appname;
char* gnx_hwnd_path;

gnx_event_handler_t gnx_sighwnd_handler = NULL;

static void gnx_sighwnd(int sig) {
    
    int fd = open(gnx_hwnd_path, O_RDONLY);
    if(fd < 0)
        goto done;
        
    gnx_event_t tmp;
    if(read(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
        close(fd);
        goto done;
    }
    
    gnx_event_t* ev = (gnx_event_t*) malloc(sizeof(tmp) + tmp.e_dlen);
    memcpy(ev, &tmp, sizeof(tmp));
    
    if(read(fd, &ev->e_data, ev->e_dlen) != ev->e_dlen) {
        free(ev);
        close(fd);
        
        goto done;
    }
    
    if(gnx_sighwnd_handler)
        gnx_sighwnd_handler(fd, ev);
     
     
    free(ev);
    close(fd);
    
done:
    signal(SIGUSR1, gnx_sighwnd);
}

int gnx_init(char* appname, gnx_event_handler_t handler) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    if(signal(SIGUSR1, gnx_sighwnd) != 0)
        return -1;
    gnx_sighwnd_handler = handler;
   
    gnxctl_send(fd, GNXCTL_TYPE_CREATE_HWND, 0, getpid(), strlen(appname) + 1, appname);
    gnxctl_close(fd);
    
    
    
    char buf[BUFSIZ];
    sprintf(buf, "/tmp/gnx/apps/%s", appname);
    
    gnx_appname = strdup(appname);
    gnx_hwnd_path = strdup(buf);
    
    return 0;
}

__attribute__ ((noreturn))
void gnx_exit(int s) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        exit(-1);
   
    gnxctl_send(fd, GNXCTL_TYPE_CLOSE_HWND, 0, 0, strlen(gnx_appname) + 1, gnx_appname);
    gnxctl_close(fd);
    
    free(gnx_appname);
    free(gnx_hwnd_path);
    exit(s);
}