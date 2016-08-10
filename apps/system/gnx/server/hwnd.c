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
#include "gnxsrv.h"


typedef struct hwnd {
    gnx_hwnd_t h_id;
    char* h_path;
    char* h_appname;
    pid_t h_pid;
    
    struct hwnd* next;
} hwnd_t;

static hwnd_t* hwnd_queue = NULL;
static gnx_hwnd_t hwnd_id = 0;

int gnxsrv_hwnd_raise(gnx_hwnd_t hid, gnx_wid_t wid, uint8_t type, gnx_param_t param, size_t dlen, void* data) {
    if(hid == -1)
        return 0;
        
    hwnd_t* hwnd;
    for(hwnd = hwnd_queue; hwnd; hwnd = hwnd->next)
        if(hwnd->h_id == hid)
            break;
    
    if(!hwnd) {
        fprintf(stderr, "gnx-server: can not raise event: invalid handle: %d\n", hid);
        return -1;
    }
    
    int fd = open(hwnd->h_path, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "gnx-server: can not raise event for %s: %s\n", hwnd->h_appname, strerror(errno));
        return -1;
    }
    
    gnx_event_t* p = (gnx_event_t*) malloc(sizeof(gnx_event_t) + dlen);
    p->e_type = type;
    p->e_wid = wid;
    p->e_param = param;
    p->e_dlen = dlen;
    
    if(dlen && data)
        memcpy(&p->e_data, data, dlen);
        
    int e = write(fd, p, sizeof(*p) + dlen) == sizeof(*p) + dlen
                ? 0
                : -1
                ;
                
    free(p);
    close(fd);
    
        
    
    if(hwnd->h_pid != -1 && kill(hwnd->h_pid, SIGUSR1) != 0) {
        fprintf(stderr, "gnx-server: (%d) SIGUSR1: %s\n", hwnd->h_pid, strerror(errno));
        return -1;
    }
    
    return e;
}


int gnxsrv_create_hwnd(char* appname, pid_t pid) {
    if(!appname) {
        fprintf(stderr, "gnx-server: appname: %s\n", strerror(EINVAL));
        return -1;
    }
    
    char buf[BUFSIZ];
    sprintf(buf, "/tmp/gnx/apps/%s.%d", appname, pid);
    
    struct stat st;
    if(stat(buf, &st) == 0) {
        fprintf(stderr, "gnx-server: %s: %s\n", buf, strerror(EEXIST));
        return -1;
    }
    
    if(mkfifo(buf, 0666) != 0) {
        fprintf(stderr, "gnx-server: %s: %s\n", buf, strerror(errno));
        return -1;
    }

    
    hwnd_t* h = (hwnd_t*) malloc(sizeof(hwnd_t));
    h->h_id = (gnx_hwnd_t) hwnd_id++;
    h->h_path = strdup(buf);
    h->h_appname = strdup(appname);
    h->h_pid = pid;
    
    h->next = hwnd_queue;
    hwnd_queue = h;
    
    gnxsrv_hwnd_raise(h->h_id, -1, GNXEV_TYPE_INIT, h->h_id, 0, NULL);
    
    
    if(verbose)
        fprintf(stdout, "gnx-server: created handle %s.%d (hwnd: %d)\n", appname, pid, h->h_id);
    
    
    return 0;
}

int gnxsrv_close_hwnd(gnx_hwnd_t hwnd) {
    
    hwnd_t* h;
    for(h = hwnd_queue; h; h = h->next)
        if(h->h_id == hwnd)
            break;
            
    if(!h) {
        fprintf(stderr, "gnx-server: unable to find handle with id %d\n", hwnd);
        return -1;
    }
    
    hwnd_t* tmp;
    for(tmp = hwnd_queue; tmp->next; tmp = tmp->next)
        if(tmp->next = h)
            break;
            
   if(!tmp && h != hwnd_queue) {
        fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
        return -1;
    }
    
    if(h == hwnd_queue)
        hwnd_queue = h->next;
    else
        tmp->next = h->next;
    
  
    
    char buf[BUFSIZ];
    sprintf(buf, "/tmp/gnx/apps/%s.%d", h->h_appname, h->h_pid);
    
    if(unlink(buf) != 0) {
        fprintf(stderr, "gnx-server: %s: %s\n", buf, strerror(errno));
        return -1;
    }
    
     if(verbose)
        fprintf(stdout, "gnx-server: closed handle %s.%d\n", h->h_appname, h->h_pid);
    
    free(h);
    return 0;
}