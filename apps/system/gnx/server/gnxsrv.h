#ifndef _GNXSRV_H
#define _GNXSRV_H

#include <sys/types.h>

extern int verbose;
int gnxsrv_init(int display);


int gnx_resources_unload(const char* name);
int gnx_resources_load(const char* name, int type);

int gnx_create_hwnd(char* appname, pid_t pid);
int gnx_close_hwnd(char* appname);

#endif
