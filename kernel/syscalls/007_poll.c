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

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>

#if defined(CONFIG_HAVE_NETWORK)
    #include <aplus/network.h>
#endif



/***
 * Name:        poll
 * Description: wait for some event on a file descriptor
 * URL:         http://man7.org/linux/man-pages/man2/poll.2.html
 *
 * Input Parameters:
 *  0: 0x07
 *  1: struct pollfd  * ufds
 *  2: unsigned int nfds
 *  3: int timeout
 *
 * Auto-generated by scripts/gen-syscalls.js
 */


SYSCALL(
    7, poll, long sys_poll(struct pollfd *ufds, unsigned int nfds, int timeout) {
        if (unlikely(!ufds))
            return -EINVAL;

        if (unlikely(!uio_check(ufds, R_OK | W_OK)))
            return -EFAULT;

        if (unlikely(nfds == 0))
            return -EINVAL;

        if (unlikely(nfds > current_task->rlimits[RLIMIT_NOFILE].rlim_cur))
            return -EINVAL;



        size_t e          = 0;
        struct pollfd pfd = {0};

        for (size_t i = 0; i < nfds; i++) {

            if (unlikely(!uio_check(&ufds[i], R_OK | W_OK)))
                return -EFAULT;


            uio_memcpy_u2s(&pfd, &ufds[i], sizeof(struct pollfd));

            if (pfd.fd < 0)
                return -EBADF;


#if defined(CONFIG_HAVE_NETWORK)

            if (NETWORK_IS_SOCKFD(pfd.fd)) {

                struct pollfd sfd;

                sfd.fd      = NETWORK_SOCKFD(pfd.fd);
                sfd.events  = pfd.events;
                sfd.revents = 0;


                if (lwip_poll_from_syscall(&sfd, 1, NULL, false) < 0)
                    return -errno;


                if (sfd.revents & pfd.events) {

                    pfd.revents |= sfd.revents & pfd.events;

                    sfd.events  = 0;
                    sfd.revents = 0;

                    uio_memcpy_s2u(&ufds[i], &pfd, sizeof(struct pollfd));

                    e++;
                }

            } else

#endif

            {

                if (pfd.fd >= CONFIG_OPEN_MAX)
                    return -EBADF;


                shared_ptr_access(current_task->fd, fds, {
                    if (fds->descriptors[pfd.fd].ref == NULL)
                        return -EBADF;

                    if (fds->descriptors[pfd.fd].ref->inode == NULL)
                        return -EBADF;


                    shared_ptr_nullable_access(fds->descriptors[pfd.fd].ref->inode->ev, ev, {
                        if (ev->revents & pfd.events) {

                            pfd.revents = ev->revents & pfd.events;

                            ev->revents &= ~pfd.events;
                            ev->events &= ~pfd.events;

                            uio_memcpy_s2u(&ufds[i], &pfd, sizeof(struct pollfd));

                            e++;
                        }
                    });
                });
            }
        }


        if (e == 0) {

            struct timespec tm;

            if (timeout > 0) {
                tm.tv_sec  = (timeout / 1000);
                tm.tv_nsec = (timeout % 1000) * 1000000;
            }


            for (size_t i = 0; i < nfds; i++) {

                uio_memcpy_u2s(&pfd, &ufds[i], sizeof(struct pollfd));

#if defined(CONFIG_HAVE_NETWORK)
                if (NETWORK_IS_SOCKFD(pfd.fd)) {

                    struct pollfd sfd;

                    sfd.fd      = NETWORK_SOCKFD(pfd.fd);
                    sfd.events  = pfd.events;
                    sfd.revents = 0;


                    if (lwip_poll_from_syscall(&sfd, 1, timeout > 0 ? &tm : NULL, true) < 0)
                        return -errno;

                } else
#endif
                {

                    shared_ptr_access(current_task->fd, fds, {
                        shared_ptr_nullable_access(fds->descriptors[pfd.fd].ref->inode->ev, ev, {
                            ev->revents &= ~pfd.events;
                            ev->events |= pfd.events;

                            futex_wait(current_task, &ev->futex, ev->futex, timeout > 0 ? &tm : NULL);
                        });
                    });
                }
            }


#if DEBUG_LEVEL_TRACE
            kprintf("poll: task %d waiting for %d events\n", current_task->tid, nfds);
#endif

            thread_suspend(current_task);
            thread_restart_sched(current_task);
            thread_restart_syscall(current_task);

            return -EINTR;
        }

        return e;
    });
