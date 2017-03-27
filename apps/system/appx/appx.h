#ifndef _APPX_H
#define _APPX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <zip.h>


#define APPX_LOCK \
    "/tmp/appx.lck"


extern int verbose;
extern int yes;

void appx_lck_acquire();
void appx_lck_release();
void appx_install_from(const char* filename);

#endif