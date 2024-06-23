/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/pty.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>

#include <aplus/utils/ringbuffer.h>

#include <ctype.h>
#include <sys/sysmacros.h>


MODULE_NAME("tty/pty");
MODULE_DEPS("dev/interface");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static uint64_t __next_pty_index = 0;

static pty_t *queue          = NULL;
static spinlock_t queue_lock = SPINLOCK_INIT;



static ssize_t pty_process_output(pty_t *pty, const char *buf, size_t size);
static ssize_t pty_process_input(pty_t *pty, const char *buf, size_t size);


__attribute__((used)) static void pty_input_discard(pty_t *pty, bool drain) {

    DEBUG_ASSERT(pty);

    if (pty->input.size == 0)
        return;


    kprintf("pty_input_discard(): discarding %zd bytes\n", pty->input.size);

    DEBUG_ASSERT(pty->input.buffer);
    DEBUG_ASSERT(pty->input.capacity);
    DEBUG_ASSERT(pty->input.capacity >= pty->input.size);

    __lock(&pty->input.lock, {
        if (drain) {
            ringbuffer_write(&pty->r1, pty->input.buffer, pty->input.size);
        }

        pty->input.size = 0;
    });
}


__attribute__((used)) static void pty_input_backspace(pty_t *pty, bool echo) {

    DEBUG_ASSERT(pty);

    if (pty->input.size == 0)
        return;


    DEBUG_ASSERT(pty->input.buffer);
    DEBUG_ASSERT(pty->input.capacity);

    __lock(&pty->input.lock, {
        if (pty->input.buffer[pty->input.size - 1] < 0x20 || pty->input.buffer[pty->input.size - 1] == 0x7F) {

            if (pty->input.size > 0) {
                pty->input.buffer[--pty->input.size] = '\0';
            }
        }

        if (pty->input.size > 0) {
            pty->input.buffer[--pty->input.size] = '\0';
        }


        if (echo) {
            pty_process_output(pty, "\b \b", 3);
        }
    });
}


__attribute__((used)) static void pty_input_append(pty_t *pty, char ch, bool echo) {

    DEBUG_ASSERT(pty);

    __lock(&pty->input.lock, {
        if (pty->input.capacity == pty->input.size) {

            pty->input.capacity += 256;
            pty->input.buffer = krealloc(pty->input.buffer, pty->input.capacity, GFP_USER);
        }


        DEBUG_ASSERT(pty->input.buffer);
        DEBUG_ASSERT(pty->input.capacity);

        pty->input.buffer[pty->input.size++] = ch;

        if (echo) {
            pty_process_output(pty, &ch, 1);
        }
    });
}


static ssize_t pty_process_output(pty_t *pty, const char *buf, size_t size) {

    DEBUG_ASSERT(pty);
    DEBUG_ASSERT(buf);

    if (unlikely(!size))
        return 0;


    for (size_t i = 0; i < size; i++) {

        char ch = buf[i];


        if (pty->ios.c_oflag & ONLCR && ch == '\n') {
            ringbuffer_write(&pty->r2, "\r", 1);
        }

        if (pty->ios.c_oflag & ONLRET && ch == '\r') {
            continue;
        }

        if (pty->ios.c_oflag & OLCUC && isalpha(ch) && islower(ch)) {
            ch += ('a' - 'A');
        }

        if (pty->ios.c_oflag & IUCLC && isalpha(ch) && isupper(ch)) {
            ch -= ('a' - 'A');
        }


        if (ringbuffer_write(&pty->r2, &ch, 1) < 0) {
            return -1;
        }
    }

    return size;
}

static ssize_t pty_process_input(pty_t *pty, const char *buf, size_t size) {

    DEBUG_ASSERT(pty);
    DEBUG_ASSERT(buf);

    if (unlikely(!size))
        return 0;


    for (size_t i = 0; i < size; i++) {

        char ch = buf[i];

        kprintf("pty_input_process(): processing char '%c' (0x%x), c_lflag: 0%o, c_iflag: 0%o, c_oflag: 0%o, c_cflag: 0%o\n", ch, ch, pty->ios.c_lflag, pty->ios.c_iflag, pty->ios.c_oflag, pty->ios.c_cflag);

        // c_iflag

        if (pty->ios.c_iflag & ISTRIP) {
            ch &= 0x7F;
        }

        if (pty->ios.c_iflag & IGNCR && ch == '\r') {
            continue;
        }

        if (pty->ios.c_iflag & INLCR && buf[i] == '\n') {
            ch = '\r';
        }

        if (pty->ios.c_iflag & ICRNL && buf[i] == '\r') {
            ch = '\n';
        }

        if (pty->ios.c_iflag & IUCLC && isalpha(ch) && isupper(ch)) {
            ch -= ('a' - 'A');
        }



        // c_lflag

        if (pty->ios.c_lflag & ISIG) {

            sig_atomic_t sig = -1;

            if (ch == pty->ios.c_cc[VINTR]) {
                sig = SIGINT;
            } else if (ch == pty->ios.c_cc[VQUIT]) {
                sig = SIGQUIT;
            } else if (ch == pty->ios.c_cc[VSUSP]) {
                sig = SIGTSTP;
            } else if (ch == pty->ios.c_cc[VEOF]) {
                sig = SIGTERM;
            }


            if (sig > 0) {

                kprintf("pty_input_process(): sending signal %d to process group %d\n", sig, pty->s_pgrp);

                if (pty->ios.c_lflag & ECHO) {

                    char cb[2] = {'^', ch + 0x40};

                    pty_process_output(pty, cb, 2);
                }

                if (!(pty->ios.c_lflag & NOFLSH)) {
                    pty_input_discard(pty, false);
                }

                if (pty->s_pgrp > 0) {
                    int e = sys_kill(-pty->s_pgrp, sig);
                    kprintf("pty_input_process(): sys_kill() returned %d (%d)\n", e, errno);
                }

                continue;
            }
        }


        if (pty->ios.c_lflag & ICANON) {
            kprintf("pty_input_process(): canonical mode\n");

            // Canonical mode

            if (ch == pty->ios.c_cc[VKILL]) {
                kprintf("pty_input_process(): VKILL\n");

                while (pty->input.size > 0) {
                    pty_input_backspace(pty, pty->ios.c_lflag & ECHOK);
                }

                if (!(pty->ios.c_lflag & ECHOK) && pty->ios.c_lflag & ECHO) {

                    char cb[2] = {'^', ch + 0x40};

                    pty_process_output(pty, cb, 2);
                }

            }


            else if (ch == pty->ios.c_cc[VERASE]) {
                kprintf("pty_input_process(): VERASE\n");

                pty_input_backspace(pty, pty->ios.c_lflag & ECHOE);

                if (!(pty->ios.c_lflag & ECHOE) && pty->ios.c_lflag & ECHO) {

                    char cb[2] = {'^', ch + 0x40};

                    pty_process_output(pty, cb, 2);
                }

            }


            else if (ch == pty->ios.c_cc[VWERASE]) {
                kprintf("pty_input_process(): VWERASE\n");

                // TODO: VWERASE: erase the word to the left of the cursor

                continue;

            }


            else if (ch == pty->ios.c_cc[VEOF]) {
                kprintf("pty_input_process(): VEOF\n");

                pty_input_discard(pty, pty->ios.c_lflag & ECHO);

            }


            else if (ch == pty->ios.c_cc[VEOL] || ch == '\n') {
                kprintf("pty_input_process(): VEOL or newline\n");

                pty_input_append(pty, '\n', pty->ios.c_lflag & ECHONL);
                pty_input_discard(pty, pty->ios.c_lflag & ECHO);

            }


            else {
                kprintf("pty_input_process(): appending char\n");

                pty_input_append(pty, ch, pty->ios.c_lflag & ECHO);
            }

        } else {
            kprintf("pty_input_process(): non-canonical mode\n");

            // Non-canonical mode (raw mode)

            if (pty->ios.c_lflag & ECHO) {
                pty_process_output(pty, &ch, 1);
            }

            ringbuffer_write(&pty->r1, &ch, 1);
        }
    }

    return size;
}



int pty_ioctl(inode_t *inode, long req, void *arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct pty *pty = (struct pty *)inode->userdata;


    switch (req) {

        case TIOCGPTN: {

            int *index = (int *)arg;

            if (unlikely(!index))
                return -EFAULT;

            if (unlikely(!uio_check(index, R_OK | W_OK)))
                return -EFAULT;


            uio_write(index, pty->index);

            return 0;
        }

        case TIOCSWINSZ: {

            struct winsize *ws = (struct winsize *)arg;

            if (unlikely(!ws))
                return -EFAULT;

            if (unlikely(!uio_check(ws, R_OK)))
                return -EFAULT;


            pty->ws.ws_row    = uio_rstruct(ws, ws_row);
            pty->ws.ws_col    = uio_rstruct(ws, ws_col);
            pty->ws.ws_xpixel = uio_rstruct(ws, ws_xpixel);
            pty->ws.ws_ypixel = uio_rstruct(ws, ws_ypixel);

            if (pty->s_pgrp > 0) {
                sys_kill(-pty->s_pgrp, SIGWINCH);
            }

            return 0;
        }

        case TIOCGWINSZ: {

            struct winsize *ws = (struct winsize *)arg;

            if (unlikely(!ws))
                return -EFAULT;

            if (unlikely(!uio_check(ws, R_OK | W_OK)))
                return -EFAULT;


            uio_wstruct(ws, ws_row, pty->ws.ws_row);
            uio_wstruct(ws, ws_col, pty->ws.ws_col);
            uio_wstruct(ws, ws_xpixel, pty->ws.ws_xpixel);
            uio_wstruct(ws, ws_ypixel, pty->ws.ws_ypixel);

            return 0;
        }

        case TIOCGPGRP: {

            pid_t *pgrp = (pid_t *)arg;

            if (unlikely(!pgrp))
                return -EFAULT;

            if (unlikely(!uio_check(pgrp, R_OK | W_OK)))
                return -EFAULT;


            uio_write(pgrp, pty->s_pgrp);

            return 0;
        }

        case TIOCSPGRP: {

            pid_t *pgrp = (pid_t *)arg;

            if (unlikely(!pgrp))
                return -EFAULT;

            if (unlikely(!uio_check(pgrp, R_OK)))
                return -EFAULT;


            pty->s_pgrp = uio_read(pgrp);

            return 0;
        }

        case TCGETS: {

            if (unlikely(!arg))
                return -EFAULT;

            if (unlikely(!uio_check(arg, R_OK | W_OK)))
                return -EFAULT;


            uio_memcpy_s2u(arg, &pty->ios, sizeof(struct termios));

            return 0;
        }

        case TCSETS:
        case TCSETSF:
        case TCSETSW: {

            if (unlikely(!arg))
                return -EFAULT;

            if (unlikely(!uio_check(arg, R_OK)))
                return -EFAULT;

            uio_memcpy_u2s(&pty->ios, arg, sizeof(struct termios));

            return 0;
        }


        case TCSBRK:
        case TCSBRKP: {

            return 0;
        }

        case TIOCSBRK:
        case TIOCCBRK: {

            return 0;
        }

        case TCXONC: {

            return 0;
        }

        case TIOCINQ: { // XXX: FIONREAD

            int *count = (int *)arg;

            if (unlikely(!count))
                return -EFAULT;

            if (unlikely(!uio_check(count, R_OK | W_OK)))
                return -EFAULT;


            uio_write(count, (int)ringbuffer_available(&pty->r1));

            return 0;
        }

        case TIOCOUTQ: {

            int *count = (int *)arg;

            if (unlikely(!count))
                return -EFAULT;

            if (unlikely(!uio_check(count, R_OK | W_OK)))
                return -EFAULT;


            uio_write(count, (int)ringbuffer_available(&pty->r2));

            return 0;
        }

        case TCFLSH: {

            // TODO: TCFLSH: flush input/output buffer
            return 0;
        }

        case TIOCSTI: {

            if (unlikely(!arg))
                return -EFAULT;

            if (unlikely(!uio_check(arg, R_OK)))
                return -EFAULT;


            return pty_process_input(pty, uio_get_ptr(arg), 1);
        }

        case TIOCCONS: {

            errno = ENOSYS;
            return -1;
        }


        case TIOCSCTTY: {

            if (unlikely(pty->m_pid > 0))
                return -EPERM;

            if (unlikely(current_task->sid != current_task->tid))
                return -EPERM;


            shared_ptr_access(current_task->ctty, ctty,
                              {
                                  if (unlikely(*ctty))
                                      return -EPERM;

                                  pty->m_pid  = current_task->tid;
                                  pty->m_sid  = current_task->sid;
                                  pty->s_pgrp = 0;

                                  *ctty = pty;
                              })

                return 0;
        }

        case TIOCNOTTY: {

            if (pty->s_pgrp != 0) {
                sys_kill(-pty->s_pgrp, SIGHUP);
            }

            shared_ptr_access(current_task->ctty, ctty, {
                if (unlikely(*ctty != pty))
                    return -EPERM;

                pty->m_pid  = 0;
                pty->m_sid  = 0;
                pty->s_pgrp = 0;

                *ctty = NULL;
            });

            return 0;
        }

        case TIOCGSID: {

            pid_t *sid = (pid_t *)arg;

            if (unlikely(!sid))
                return -EFAULT;

            if (unlikely(!uio_check(sid, R_OK | W_OK)))
                return -EFAULT;


            uio_write(sid, pty->m_sid);

            return 0;
        }


        case TIOCEXCL:
        case TIOCNXCL: {

            return 0;
        }

        case TIOCGEXCL: {

            int *excl = (int *)arg;

            if (unlikely(!excl))
                return -EFAULT;

            if (unlikely(!uio_check(excl, R_OK | W_OK)))
                return -EFAULT;


            uio_write(excl, 0);

            return 0;
        }


        case TIOCGETD:
        case TIOCSETD: {

            errno = ENOSYS;
            return -1;
        }


        case TIOCSPTLCK: {

            int *lock = (int *)arg;

            if (unlikely(!lock))
                return -EFAULT;

            if (unlikely(!uio_check(lock, R_OK)))
                return -EFAULT;


            pty->locked = !!(uio_read(lock));

            return 0;
        }

        case TIOCGPTLCK: {

            int *lock = (int *)arg;

            if (unlikely(!lock))
                return -EFAULT;

            if (unlikely(!uio_check(lock, R_OK | W_OK)))
                return -EFAULT;


            uio_write(lock, pty->locked);

            return 0;
        }


        default:

#if DEBUG_LEVEL_WARN
            kprintf("ptyfs: WARN! ioctl(%ld, %p) not implemented\n", req, arg);
#endif

            break;
    }


    errno = EINVAL;
    return -1;
}



ssize_t pty_master_read(inode_t *inode, void *buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct pty *pty = (struct pty *)inode->userdata;

    if (unlikely(!buf))
        return -EFAULT;

    if (unlikely(!uio_check(buf, R_OK | W_OK)))
        return -EFAULT;

    if (unlikely(!size))
        return 0;


    return ringbuffer_read(&pty->r2, buf, size);
}


ssize_t pty_master_write(inode_t *inode, const void *buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct pty *pty = (struct pty *)inode->userdata;

    if (unlikely(!buf))
        return -EFAULT;

    if (unlikely(!uio_check(buf, R_OK)))
        return -EFAULT;

    if (unlikely(!size))
        return 0;


    return pty_process_input(pty, buf, size);
}


ssize_t pty_slave_read(inode_t *inode, void *buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct pty *pty = (struct pty *)inode->userdata;

    if (unlikely(!buf))
        return -EFAULT;

    if (unlikely(!uio_check(buf, R_OK | W_OK)))
        return -EFAULT;

    if (unlikely(!size))
        return 0;


    if (pty->ios.c_lflag & ICANON) {

        return ringbuffer_read(&pty->r1, buf, size);

    } else {

        if (pty->ios.c_cc[VMIN] > 0) {

            return ringbuffer_read(&pty->r1, buf, MIN(size, pty->ios.c_cc[VMIN]));

        } else {

            return ringbuffer_read(&pty->r1, buf, MIN(size, ringbuffer_available(&pty->r1)));
        }
    }
}


ssize_t pty_slave_write(inode_t *inode, const void *buf, off_t offset, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);

    struct pty *pty = (struct pty *)inode->userdata;

    if (unlikely(!buf))
        return -EFAULT;

    if (unlikely(!uio_check(buf, R_OK)))
        return -EFAULT;

    if (unlikely(!size))
        return 0;


    return pty_process_output(pty, buf, size);
}


pty_t *pty_create(inode_t *ptmx, int flags) {

    DEBUG_ASSERT(ptmx);


    struct pty *pty = (struct pty *)kcalloc(sizeof(struct pty), 1, GFP_USER);

    if (unlikely(!pty))
        return NULL;


    pty->index = __next_pty_index++;

    pty->m_pid  = current_task->tid;
    pty->s_pgrp = 0;

    pty->ios.c_cc[VINTR]    = 0x03;
    pty->ios.c_cc[VQUIT]    = 0x1C;
    pty->ios.c_cc[VERASE]   = 0x7F;
    pty->ios.c_cc[VKILL]    = 0x15;
    pty->ios.c_cc[VEOF]     = 0x04;
    pty->ios.c_cc[VTIME]    = 0;
    pty->ios.c_cc[VMIN]     = 1;
    pty->ios.c_cc[VSTART]   = 0x11;
    pty->ios.c_cc[VSTOP]    = 0x13;
    pty->ios.c_cc[VSUSP]    = 0x1A;
    pty->ios.c_cc[VEOL]     = 0;
    pty->ios.c_cc[VREPRINT] = 0x12;
    pty->ios.c_cc[VDISCARD] = 0x0F;
    pty->ios.c_cc[VWERASE]  = 0x17;
    pty->ios.c_cc[VLNEXT]   = 0x16;
    pty->ios.c_cc[VEOL2]    = 0;

    pty->ios.c_iflag = ICRNL | IXON | IXOFF;
    pty->ios.c_oflag = OPOST | ONLCR;
    pty->ios.c_cflag = B38400 | CS8 | CREAD | HUPCL;
    pty->ios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOKE | IEXTEN;


    pty->ws.ws_row    = 25;
    pty->ws.ws_col    = 80;
    pty->ws.ws_xpixel = 0;
    pty->ws.ws_ypixel = 0;


    pty->input.buffer   = NULL;
    pty->input.size     = 0;
    pty->input.capacity = 0;

    spinlock_init_with_flags(&pty->input.lock, SPINLOCK_FLAGS_RECURSIVE);


    ringbuffer_init(&pty->r1, CONFIG_BUFSIZ * 64);
    ringbuffer_init(&pty->r2, CONFIG_BUFSIZ * 64);



    pty->ptmx = ptmx;

    __lock(&queue_lock, {
        pty->next = queue;
        queue     = pty;
    });

    return pty;
}

pty_t *pty_queue() {
    return queue;
}

void pty_queue_lock() {
    spinlock_lock(&queue_lock);
}

void pty_queue_unlock() {
    spinlock_unlock(&queue_lock);
}


void init(const char *args) {
    spinlock_init(&queue_lock);
}

void dnit(void) {
}
