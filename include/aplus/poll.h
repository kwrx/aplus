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

#ifndef _APLUS_POLL_H
#define _APLUS_POLL_H

#ifndef __ASSEMBLY__

    #include <poll.h>
    #include <signal.h>
    #include <stdbool.h>
    #include <stdint.h>
    #include <time.h>

    #include <sys/select.h>
    #include <sys/time.h>

    #include <aplus.h>


//? The readiness core behind poll(), ppoll(), select() and pselect6(). All four
//? ask the same two questions -- "is this descriptor ready?" and "wake me when
//? it is" -- and differ only in how the caller spells the descriptor list, the
//? timeout and the signal mask.


//? One past the highest descriptor that can ever exist: files live below
//? CONFIG_OPEN_MAX and sockets are folded in just above it.
    #if defined(CONFIG_HAVE_NETWORK)
        #define POLL_FD_MAX (CONFIG_OPEN_MAX + CONFIG_SOCKET_MAX)
    #else
        #define POLL_FD_MAX (CONFIG_OPEN_MAX)
    #endif


//? A wait with no timeout at all, as opposed to one with nothing left to wait.
    #define POLL_TIMEOUT_FOREVER ((uint64_t)-1)


//? How select() splits a descriptor's readiness across its three sets. POSIX
//? leaves "ready" to the implementation; these are the mappings Linux uses, so
//? a hangup or an error wakes a reader rather than stranding it.
    #define POLL_SET_IN  (POLLIN | POLLRDNORM | POLLRDBAND | POLLHUP | POLLERR)
    #define POLL_SET_OUT (POLLOUT | POLLWRNORM | POLLWRBAND | POLLERR)
    #define POLL_SET_EX  (POLLPRI)


//? What is left of a timeout that has to survive the syscall being restarted.
typedef enum {

    POLL_DEADLINE_FOREVER,   //? No timeout was asked for: only a descriptor wakes it.
    POLL_DEADLINE_REMAINING, //? Some time is left; the caller sleeps for that much.
    POLL_DEADLINE_EXPIRED,   //? Nothing left: the caller reports a timeout.

} poll_deadline_t;


__BEGIN_DECLS

//* kernel/fs/poll.c

int poll_scan(int fd, short events, short* revents);
int poll_arm(int fd, short events, struct timespec* timeout, bool* armed);

poll_deadline_t poll_deadline(uint64_t timeout_ns, struct timespec* remaining);

long poll_suspend(bool armed, struct timespec* timeout);
long poll_finish(long retval);

int poll_sigmask_install(const sigset_t* sigmask, size_t sigsetsize);

int poll_timeout_timespec(const struct timespec* tsp, uint64_t* timeout_ns);
int poll_timeout_timeval(const struct timeval* tvp, uint64_t* timeout_ns);

long poll_wait_pollfd(struct pollfd* ufds, unsigned int nfds, uint64_t timeout_ns);
long poll_wait_fdset(int n, fd_set* inp, fd_set* outp, fd_set* exp, uint64_t timeout_ns);

__END_DECLS

#endif
#endif
