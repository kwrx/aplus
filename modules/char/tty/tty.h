#ifndef _TTY_H
#define _TTY_H

#include <xdev.h>
#include <libc.h>

#define TTY_DEFAULT_INPUT_DEVICE    "/dev/kbd"


int tty_write(struct inode* inode, void* ptr, size_t len);
int tty_read(struct inode* inode, void* ptr, size_t len);

#endif