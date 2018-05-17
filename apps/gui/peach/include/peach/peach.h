#ifndef _PEACH_H
#define _PEACH_H

#include <stdint.h>
#include <aplus/base.h>
#include <aplus/fb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t magic;
    uint16_t type;
    uint16_t size;
    char data[0];
} peach_msg_t;

typedef struct {
    peach_msg_t header;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
} peach_msg_welcome_t;


#define PEACH_MSG_MAGIC                     0xCAFE
#define PEACH_MSG_SUBSCRIBE                 0x0100
#define PEACH_MSG_WELCOME                   0x0101


int peach_subscribe(int[2], int);

#ifdef __cplusplus
}
#endif


#endif