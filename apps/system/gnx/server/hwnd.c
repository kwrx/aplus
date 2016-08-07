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

int gnx_hwnd_raise(hwnd_t* hwnd, gnx_wid_t wid, uint8_t type, int param, size_t dlen, void* data) {
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
    
        
    
    if(pid != -1 && kill(pid, SIGUSR1) != 0) {
        fprintf(stderr, "gnx-server: (%d) SIGUSR1: %s\n", pid, strerror(errno));
        return -1;
    }
    
    return e;
}


int gnx_create_hwnd(char* appname, pid_t pid) {
    if(!appname) {
        fprintf(stderr, "gnx-server: appname: %s\n", strerror(EINVAL));
        return -1;
    }
    
    char buf[BUFSIZ];
    sprintf(buf, "/tmp/gnx/apps/%s", appname);
    
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
    
    gnx_hwnd_raise(h, -1, GNXEV_TYPE_INIT, h->h_id, 0, NULL);
    
    
    if(verbose)
        fprintf(stdout, "gnx-server: created handle %s\n", appname);
    
    
    return 0;
}

int gnx_close_hwnd(char* appname) {
    if(!appname) {
        fprintf(stderr, "gnx-server: appname: %s\n", strerror(EINVAL));
        return -1;
    }
    
    hwnd_t* tmp;
    for(tmp = hwnd_queue; tmp->next; tmp = tmp->next)
        if(strcmp(tmp->next->h_appname, appname) == 0)
            break;
            
    if(!tmp->next && (strcmp(hwnd_queue->h_appname, appname) == 0)) {
        tmp = hwnd_queue;
        hwnd_queue = hwnd_queue->next;
        
        free(tmp);
    } else
        return -1;
    
    hwnd_t* t0 = tmp->next;
    tmp->next = tmp->next->next;
    free(t0);
    
    
    char buf[BUFSIZ];
    sprintf(buf, "/tmp/gnx/apps/%s", appname);
    
    if(unlink(buf) != 0) {
        fprintf(stderr, "gnx-server: %s: %s\n", buf, strerror(errno));
        return -1;
    }
    
     if(verbose)
        fprintf(stdout, "gnx-server: closed handle %s\n", appname);
    
    
    return 0;
}