#ifndef _PEACH_H
#define _PEACH_H



#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t magic;
    uint16_t type;
    uint16_t size;
    char data[0];
} peach_msg_t;


#define PEACH_MSG_MAGIC                     0xCAFE
#define PEACH_MSG_SUBSCRIBE                 0x0100


int peach_subscribe(int[2], int);

#ifdef __cplusplus
}
#endif


#endif