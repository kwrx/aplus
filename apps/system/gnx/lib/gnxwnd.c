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


int gnx_window(gnx_wid_t parent) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    gnxctl_send(fd, GNXCTL_TYPE_CREATE_WINDOW, gnx_hwnd, parent, 0, NULL);
    gnxctl_close(fd);
}

int gnx_window_close(gnx_wid_t wid) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    gnxctl_send(fd, GNXCTL_TYPE_CLOSE_WINDOW, gnx_hwnd, wid, 0, NULL);
    gnxctl_close(fd);
}

int gnx_window_blit(gnx_wid_t wid) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    gnxctl_send(fd, GNXCTL_TYPE_BLIT_WINDOW, gnx_hwnd, wid, 0, NULL);
    gnxctl_close(fd);
}

int gnx_window_set_font(gnx_wid_t wid, char* fontface) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    gnxctl_send(fd, GNXCTL_TYPE_SET_WINDOW_FONT, gnx_hwnd, wid, strlen(fontface) + 1, fontface);
    gnxctl_close(fd);    
}

int gnx_window_set_title(gnx_wid_t wid, char* title) {
    int fd;
    if((fd = gnxctl_open()) < 0)
        return -1;
        
    gnxctl_send(fd, GNXCTL_TYPE_SET_WINDOW_TITLE, gnx_hwnd, wid, strlen(title) + 1, title);
    gnxctl_close(fd);    
}