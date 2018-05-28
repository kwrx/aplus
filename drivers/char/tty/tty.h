/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _TTY_H
#define _TTY_H

#include <aplus.h>
#include <aplus/base.h>
#include <libc.h>

#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/ttydefaults.h>
#include <sys/ioctl.h>
#include <sys/wait.h>


#define TTY_DEFAULT_INPUT_DEVICE    PATH_KBDEV
#define TTY_DEFAULT_OUTPUT_DEVICE   PATH_CONDEV
#define TTY_BUFSIZ                  65536



int tty_output_write(struct inode* inode, void* ptr, off_t pos, size_t len);
int tty_input_write(struct inode* inode, void* ptr, off_t pos, size_t len);
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

    char outbuf[TTY_BUFSIZ];
    int outlen;
    
    fifo_t in;
    fifo_t uin;
    pid_t pgrp;
};

int tty_read_init();
int tty_load_keymap(char* keymap);
void __tty_drain(struct tty_context* tio);
void __tty_flush(struct tty_context* tio, int mode);


#endif