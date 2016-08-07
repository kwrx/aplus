#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <aplus/gnx.h>

int gnxctl_open(void) {
    return open("/tmp/gnx/gnxctl", O_RDWR);
}


int gnxctl_close(int fd) {
    return close(fd);
}

int gnxctl_send(int fd, int type, int hwnd, int param, int dlen, void* data) {
    gnxctl_packet_t* p = (gnxctl_packet_t*) malloc(sizeof(gnxctl_packet_t) + dlen);
    p->g_type = type;
    p->g_hwnd = hwnd;
    p->g_param = param;
    p->g_dlen = dlen;
    
    if(dlen && data)
        memcpy(&p->g_data, data, dlen);
        
    int e = write(fd, p, sizeof(*p) + dlen) == sizeof(*p) + dlen
                ? 0
                : -1
                ;
                
    free(p);
    return e;
}

int gnxctl_recv(int fd, gnxctl_packet_t** p) {
    if(!p)
        return -1;
        
    gnxctl_packet_t tmp;
    if(read(fd, &tmp, sizeof(tmp)) != sizeof(tmp))
        return -1;
        
    *p = malloc(sizeof(tmp) + tmp.g_dlen);
    if(!*p)
        return -1;
    
    
    
    (*p)->g_type = tmp.g_type;
    (*p)->g_hwnd = tmp.g_hwnd;
    (*p)->g_param = tmp.g_param;
    (*p)->g_dlen = tmp.g_dlen;
    
    if(read(fd, &((*p)->g_data), tmp.g_dlen) != tmp.g_dlen)
        return -1;
    
    return 0;
}