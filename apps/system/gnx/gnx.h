#ifndef _GNX_H
#define _GNX_H

#define GNX_MAX_DISPLAY     4


#define GNXCTL_TYPE_INIT_DISPLAY            0
#define GNXCTL_TYPE_FINI_DISPLAY            1

typedef struct {
    int g_type;
    int g_hwnd;
    int g_param;
    int g_dlen;
    char g_data[0];
} __attribute__ ((packed)) gnxctl_packet_t;



extern int verbose;
int gnx_init(int display);


int gnxctl_open(void);
int gnxctl_close(int fd);
int gnxctl_send(int fd, int type, int hwnd, int param, int dlen, void* data);
int gnxctl_recv(int fd, gnxctl_packet_t** p);


#endif
