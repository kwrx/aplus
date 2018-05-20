#ifndef _TTY_H
#define _TTY_H

#include <aplus.h>
#include <aplus/base.h>
#include <libc.h>

#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/ioctl.h>

#define TTY_DEFAULT_INPUT_DEVICE    PATH_KBDEV
#define TTY_DEFAULT_OUTPUT_DEVICE   PATH_CONDEV



int tty_write(struct inode* inode, void* ptr, off_t pos, size_t len);
int tty_read(struct inode* inode, void* ptr, off_t pos, size_t len);
int tty_ioctl(struct inode* inode, int req, void* data);


struct tty_context {
    union {
        struct termio io;
        struct termios ios;
    };

    struct winsize winsize;
    int lined;
    int output;

    char outbuf[65536];
    int outlen;
    
    fifo_t in;
    pid_t pgrp;
};

int tty_read_init();
int tty_load_keymap(char* keymap);
void __tty_flush(struct tty_context* tio);


#endif