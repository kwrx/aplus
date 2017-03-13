#ifndef _TTY_H
#define _TTY_H

#include <aplus.h>
#include <aplus/base.h>
#include <libc.h>

#define TTY_DEFAULT_INPUT_DEVICE    PATH_KBDEV


extern char tty_keymap[1024];

int tty_write(struct inode* inode, void* ptr, size_t len);
int tty_read(struct inode* inode, void* ptr, size_t len);
int tty_read_init();

#endif