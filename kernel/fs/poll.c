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
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/poll.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include <aplus/network.h>


/**
 * @brief Readiness is asked for, never remembered: every attempt rescans from scratch.
 *
 * Waiting is two passes -- every descriptor registers an interest first, then the task suspends once.
 */


#define POLL_FDSET_BITS     (8 * sizeof(unsigned long))
#define POLL_FDSET_WORDS(n) (((n) + POLL_FDSET_BITS - 1) / POLL_FDSET_BITS)
#define POLL_FDSET_MAX      POLL_FDSET_WORDS(POLL_FD_MAX)

#define POLL_FDSET_ISSET(fd, set) (((set)[(fd) / POLL_FDSET_BITS] >> ((fd) % POLL_FDSET_BITS)) & 1UL)
#define POLL_FDSET_SET(fd, set)   ((set)[(fd) / POLL_FDSET_BITS] |= (1UL << ((fd) % POLL_FDSET_BITS)))


/**
 * @brief Asks a single descriptor whether it is ready.
 *
 * @param fd Descriptor to interrogate; must not be negative.
 * @param events Mask of the events the caller cares about.
 * @param revents Receives what is actually ready, or POLLNVAL if @p fd is not open.
 * @return 0, or a negative error number.
 */
int poll_scan(int fd, short events, short* revents) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(revents);
    DEBUG_ASSERT(fd >= 0);

    *revents = 0;


    if (fd >= CONFIG_OPEN_MAX)
        return *revents = POLLNVAL, 0;


    shared_ptr_access(current_task->fd, fds, {
        if (fds->descriptors[fd].ref == NULL || fds->descriptors[fd].ref->inode == NULL) {

            *revents = POLLNVAL;

        } else {

            *revents = vfs_poll(fds->descriptors[fd].ref->inode, events);
        }
    });

    return 0;
}


/**
 * @brief Registers the current task to be woken when a descriptor moves.
 *
 * Registers only: the caller suspends once after arming everything it watches.
 *
 * @param fd Descriptor to watch; must not be negative.
 * @param events Mask of the events the caller cares about.
 * @param timeout Time left to sleep, or NULL to wait indefinitely.
 * @param armed Set to true if the descriptor could actually be watched.
 * @return 0, or a negative error number.
 */
int poll_arm(int fd, short events, struct timespec* timeout, bool* armed) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(armed);
    DEBUG_ASSERT(fd >= 0);

    *armed = false;


    if (fd >= CONFIG_OPEN_MAX)
        return 0;


    int e = 0;

    shared_ptr_access(current_task->fd, fds, {
        if (fds->descriptors[fd].ref != NULL && fds->descriptors[fd].ref->inode != NULL) {

            inode_t* inode = fds->descriptors[fd].ref->inode;

            bool handled = false;


            int r = socket_poll_arm(inode, events, timeout);

            if (r != 0) {

                handled = true;

                if (r > 0)
                    *armed = true;
                else
                    e = r;
            }


            if (!handled) {

                shared_ptr_nullable_access(inode->ev, ev, {
                    futex_wait(current_task, &ev->futex, ev->futex, timeout);
                    *armed = true;
                });
            }
        }
    });

    return e;
}


/**
 * @brief Works out how much of a timeout is left, from a deadline stamped once and reused by every restart.
 *
 * @param timeout_ns Relative timeout in nanoseconds, or POLL_TIMEOUT_FOREVER.
 * @param remaining Receives the time left when POLL_DEADLINE_REMAINING is returned.
 * @return Whether the deadline has expired, has time left, or never expires.
 */
poll_deadline_t poll_deadline(uint64_t timeout_ns, struct timespec* remaining) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(remaining);


    if (timeout_ns == POLL_TIMEOUT_FOREVER)
        return POLL_DEADLINE_FOREVER;


    if (!current_task->syscall.deadline_valid) {

        uint64_t now = arch_timer_generic_getns() + timeout_ns;

        current_task->syscall.deadline.tv_sec  = (time_t)(now / 1000000000ULL);
        current_task->syscall.deadline.tv_nsec = (long)(now % 1000000000ULL);
        current_task->syscall.deadline_valid   = true;
    }


    uint64_t deadline = ((uint64_t)current_task->syscall.deadline.tv_sec * 1000000000ULL) + (uint64_t)current_task->syscall.deadline.tv_nsec;
    uint64_t now      = arch_timer_generic_getns();

    if (now >= deadline)
        return POLL_DEADLINE_EXPIRED;


    remaining->tv_sec  = (time_t)((deadline - now) / 1000000000ULL);
    remaining->tv_nsec = (long)((deadline - now) % 1000000000ULL);

    return POLL_DEADLINE_REMAINING;
}


/**
 * @brief Suspends the current task until something it armed moves, or time runs out.
 *
 * @param armed Whether anything was armed at all.
 * @param timeout Time left to sleep, or NULL to wait indefinitely.
 * @return -EINTR, with the syscall marked for restart.
 */
long poll_suspend(bool armed, struct timespec* timeout) {

    DEBUG_ASSERT(current_task);


    if (!armed && timeout != NULL)
        futex_wait(current_task, &current_task->syscall.deadline_futex, current_task->syscall.deadline_futex, timeout);


#if DEBUG_LEVEL_TRACE
    kprintf("poll: task %d waiting for events\n", current_task->tid);
#endif

    thread_suspend(current_task);
    thread_restart_sched(current_task);
    thread_restart_syscall(current_task);

    return -EINTR;
}


/**
 * @brief Drops the per-attempt state a wait leaves behind and hands back its result.
 *
 * @param retval The result to hand back.
 * @return The result passed in.
 */
long poll_finish(long retval) {

    DEBUG_ASSERT(current_task);


    if (current_task->flags & TASK_FLAGS_NEED_SYSCALL_RESTART)
        return retval;


    current_task->syscall.deadline_valid = false;

    if (current_task->syscall.sigmask_valid) {

        shared_ptr_access(current_task->sighand, sighand, { memcpy(&sighand->sigmask, &current_task->syscall.sigmask, sizeof(sigset_t)); });

        current_task->syscall.sigmask_valid = false;
    }

    return retval;
}


/**
 * @brief Blocks the signals a wait was asked to ignore, remembering what to put back.
 *
 * @param sigmask Mask to install, or NULL to leave the current one alone.
 * @param sigsetsize Size of @p sigmask, as the caller understands it.
 * @return 0, or a negative error number.
 */
int poll_sigmask_install(const sigset_t* sigmask, size_t sigsetsize) {

    DEBUG_ASSERT(current_task);


    if (sigmask == NULL)
        return 0;

    if (unlikely(sigsetsize > sizeof(sigset_t)))
        return -EINVAL;

    if (unlikely(sigsetsize % sizeof(unsigned long)))
        return -EINVAL;

    if (unlikely(!uio_check(sigmask, R_OK)))
        return -EFAULT;


    if (current_task->syscall.sigmask_valid)
        return 0;


    sigset_t safe;
    memset(&safe, 0, sizeof(sigset_t));

    uio_memcpy_u2s(&safe, sigmask, sigsetsize);

    shared_ptr_access(current_task->sighand, sighand, {
        memcpy(&current_task->syscall.sigmask, &sighand->sigmask, sizeof(sigset_t));
        memcpy(&sighand->sigmask, &safe, sizeof(sigset_t));
    });

    current_task->syscall.sigmask_valid = true;

    return 0;
}


/**
 * @brief The largest timeout that is neither mistakable for "never" nor wraps in nanoseconds.
 */
#define POLL_TIMEOUT_SEC_MAX ((time_t)(((uint64_t)-2) / 1000000000ULL))


/**
 * @brief Reads a ppoll()/pselect6() timeout in from user memory.
 *
 * @param tsp Caller's timeout, or NULL to wait indefinitely.
 * @param timeout_ns Receives the timeout in nanoseconds, or POLL_TIMEOUT_FOREVER.
 * @return 0, or a negative error number.
 */
int poll_timeout_timespec(const struct timespec* tsp, uint64_t* timeout_ns) {

    DEBUG_ASSERT(timeout_ns);


    if (tsp == NULL)
        return *timeout_ns = POLL_TIMEOUT_FOREVER, 0;

    if (unlikely(!uio_check(tsp, R_OK)))
        return -EFAULT;


    struct timespec ts;
    uio_memcpy_u2s(&ts, tsp, sizeof(struct timespec));

    if (unlikely(ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L))
        return -EINVAL;


    if (unlikely(ts.tv_sec > POLL_TIMEOUT_SEC_MAX))
        return *timeout_ns = POLL_TIMEOUT_FOREVER - 1, 0;

    *timeout_ns = ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;

    return 0;
}


/**
 * @brief Reads a select() timeout in from user memory.
 *
 * @param tvp Caller's timeout, or NULL to wait indefinitely.
 * @param timeout_ns Receives the timeout in nanoseconds, or POLL_TIMEOUT_FOREVER.
 * @return 0, or a negative error number.
 */
int poll_timeout_timeval(const struct timeval* tvp, uint64_t* timeout_ns) {

    DEBUG_ASSERT(timeout_ns);


    if (tvp == NULL)
        return *timeout_ns = POLL_TIMEOUT_FOREVER, 0;

    if (unlikely(!uio_check(tvp, R_OK)))
        return -EFAULT;


    struct timeval tv;
    uio_memcpy_u2s(&tv, tvp, sizeof(struct timeval));

    if (unlikely(tv.tv_sec < 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000L))
        return -EINVAL;


    if (unlikely(tv.tv_sec > POLL_TIMEOUT_SEC_MAX))
        return *timeout_ns = POLL_TIMEOUT_FOREVER - 1, 0;

    *timeout_ns = ((uint64_t)tv.tv_sec * 1000000000ULL) + ((uint64_t)tv.tv_usec * 1000ULL);

    return 0;
}


/**
 * @brief Asks every descriptor in a pollfd array whether it is ready, writing the answers back.
 *
 * @param ufds Caller's descriptor array, in user memory.
 * @param nfds Number of entries in @p ufds.
 * @return The number of ready descriptors, zero if none are, or a negative error number.
 */
static long __poll_scan_pollfd(struct pollfd* ufds, unsigned int nfds) {

    size_t ready = 0;

    for (size_t i = 0; i < nfds; i++) {

        struct pollfd pfd;

        if (unlikely(!uio_check(&ufds[i], R_OK | W_OK)))
            return -EFAULT;

        uio_memcpy_u2s(&pfd, &ufds[i], sizeof(struct pollfd));

        pfd.revents = 0;

        if (pfd.fd >= 0) {

            int err;

            if ((err = poll_scan(pfd.fd, pfd.events, &pfd.revents)) < 0)
                return err;

            if (pfd.revents)
                ready++;
        }

        uio_memcpy_s2u(&ufds[i], &pfd, sizeof(struct pollfd));
    }

    return (long)ready;
}


/**
 * @brief The body behind poll() and ppoll().
 *
 * The descriptors are asked once more after the interest has been registered, then the task suspends.
 *
 * @param ufds Caller's descriptor array, in user memory.
 * @param nfds Number of entries in @p ufds.
 * @param timeout_ns Relative timeout in nanoseconds, or POLL_TIMEOUT_FOREVER.
 * @return Number of ready descriptors, 0 on timeout, or a negative error number.
 */
long poll_wait_pollfd(struct pollfd* ufds, unsigned int nfds, uint64_t timeout_ns) {

    DEBUG_ASSERT(current_task);


    if (unlikely(nfds > current_task->rlimits[RLIMIT_NOFILE].rlim_cur))
        return -EINVAL;

    if (unlikely(nfds && !ufds))
        return -EINVAL;

    if (unlikely(nfds && !uio_check(ufds, R_OK | W_OK)))
        return -EFAULT;


    long ready;

    if ((ready = __poll_scan_pollfd(ufds, nfds)) != 0)
        return ready;


    struct timespec tm  = {0, 0};
    struct timespec* to = NULL;

    switch (poll_deadline(timeout_ns, &tm)) {

        case POLL_DEADLINE_EXPIRED:
            return 0;

        case POLL_DEADLINE_REMAINING:
            to = &tm;
            break;

        case POLL_DEADLINE_FOREVER:
            break;
    }


    bool armed = false;

    for (size_t i = 0; i < nfds; i++) {

        struct pollfd pfd;

        if (unlikely(!uio_check(&ufds[i], R_OK | W_OK)))
            return -EFAULT;

        uio_memcpy_u2s(&pfd, &ufds[i], sizeof(struct pollfd));

        if (pfd.fd < 0)
            continue;


        int err;
        bool one;

        if ((err = poll_arm(pfd.fd, pfd.events, to, &one)) < 0)
            return err;

        armed |= one;
    }


    if ((ready = __poll_scan_pollfd(ufds, nfds)) != 0)
        return ready;


    return poll_suspend(armed, to);
}


/**
 * @brief Copies the meaningful words of one of select()'s sets in from user memory.
 *
 * @param uset Caller's set, or NULL, which reads back as all-zero.
 * @param words Number of words covering the descriptors the caller asked about.
 * @param kset Receives the copy.
 * @return 0, or a negative error number.
 */
static int poll_fdset_get(const fd_set* uset, size_t words, unsigned long* kset) {

    memset(kset, 0, words * sizeof(unsigned long));

    if (uset == NULL)
        return 0;

    for (size_t i = 0; i < words; i++) {

        if (unlikely(!uio_check(&uset->fds_bits[i], R_OK)))
            return -EFAULT;

        uio_memcpy_u2s(&kset[i], &uset->fds_bits[i], sizeof(unsigned long));
    }

    return 0;
}


/**
 * @brief Copies one of select()'s answers back out to user memory.
 *
 * @param uset Caller's set, or NULL to discard the answer.
 * @param words Number of words to write back.
 * @param kset The answer to copy out.
 * @return 0, or a negative error number.
 */
static int poll_fdset_put(fd_set* uset, size_t words, const unsigned long* kset) {

    if (uset == NULL)
        return 0;

    for (size_t i = 0; i < words; i++) {

        if (unlikely(!uio_check(&uset->fds_bits[i], R_OK | W_OK)))
            return -EFAULT;

        uio_memcpy_s2u(&uset->fds_bits[i], &kset[i], sizeof(unsigned long));
    }

    return 0;
}


/**
 * @brief Asks every descriptor in select()'s three sets whether it is ready.
 *
 * @param n One past the highest descriptor to look at.
 * @param in Descriptors the caller asked about for reading.
 * @param out Descriptors the caller asked about for writing.
 * @param ex Descriptors the caller asked about for exceptional conditions.
 * @param rin Receives the descriptors ready for reading.
 * @param rout Receives the descriptors ready for writing.
 * @param rex Receives the descriptors with an exceptional condition.
 * @return The number of ready bits, counting a descriptor once per set it is ready in, or a negative error number.
 */
static long __poll_scan_fdset(int n, const unsigned long* in, const unsigned long* out, const unsigned long* ex, unsigned long* rin, unsigned long* rout, unsigned long* rex) {

    size_t ready = 0;

    for (int fd = 0; fd < n; fd++) {

        short events = 0;

        if (POLL_FDSET_ISSET(fd, in))
            events |= POLL_SET_IN;

        if (POLL_FDSET_ISSET(fd, out))
            events |= POLL_SET_OUT;

        if (POLL_FDSET_ISSET(fd, ex))
            events |= POLL_SET_EX;

        if (!events)
            continue;


        int err;
        short revents;

        if ((err = poll_scan(fd, events, &revents)) < 0)
            return err;

        if (revents & POLLNVAL)
            return -EBADF;


        if ((revents & POLL_SET_IN) && POLL_FDSET_ISSET(fd, in))
            POLL_FDSET_SET(fd, rin), ready++;

        if ((revents & POLL_SET_OUT) && POLL_FDSET_ISSET(fd, out))
            POLL_FDSET_SET(fd, rout), ready++;

        if ((revents & POLL_SET_EX) && POLL_FDSET_ISSET(fd, ex))
            POLL_FDSET_SET(fd, rex), ready++;
    }

    return (long)ready;
}


/**
 * @brief The body behind select() and pselect6().
 *
 * The caller's sets are left untouched unless the call is really returning, since a restart re-reads them.
 *
 * @param n One past the highest descriptor to look at.
 * @param inp Descriptors to watch for reading, or NULL.
 * @param outp Descriptors to watch for writing, or NULL.
 * @param exp Descriptors to watch for exceptional conditions, or NULL.
 * @param timeout_ns Relative timeout in nanoseconds, or POLL_TIMEOUT_FOREVER.
 * @return Number of ready descriptors, 0 on timeout, or a negative error number.
 */
long poll_wait_fdset(int n, fd_set* inp, fd_set* outp, fd_set* exp, uint64_t timeout_ns) {

    DEBUG_ASSERT(current_task);


    if (unlikely(n < 0))
        return -EINVAL;

    if (n > POLL_FD_MAX)
        n = POLL_FD_MAX;


    size_t words = POLL_FDSET_WORDS((size_t)n);

    unsigned long in[POLL_FDSET_MAX], out[POLL_FDSET_MAX], ex[POLL_FDSET_MAX];
    unsigned long rin[POLL_FDSET_MAX], rout[POLL_FDSET_MAX], rex[POLL_FDSET_MAX];

    int err;

    if ((err = poll_fdset_get(inp, words, in)) < 0)
        return err;

    if ((err = poll_fdset_get(outp, words, out)) < 0)
        return err;

    if ((err = poll_fdset_get(exp, words, ex)) < 0)
        return err;

    memset(rin, 0, sizeof(rin));
    memset(rout, 0, sizeof(rout));
    memset(rex, 0, sizeof(rex));


    long ready;

    if ((ready = __poll_scan_fdset(n, in, out, ex, rin, rout, rex)) < 0)
        return ready;


    struct timespec tm  = {0, 0};
    struct timespec* to = NULL;

    if (ready == 0) {

        switch (poll_deadline(timeout_ns, &tm)) {

            case POLL_DEADLINE_EXPIRED:
                break;

            case POLL_DEADLINE_REMAINING:
                to = &tm;
                goto wait;

            case POLL_DEADLINE_FOREVER:
                goto wait;
        }
    }


answer:

    if ((err = poll_fdset_put(inp, words, rin)) < 0)
        return err;

    if ((err = poll_fdset_put(outp, words, rout)) < 0)
        return err;

    if ((err = poll_fdset_put(exp, words, rex)) < 0)
        return err;

    return ready;


wait:

    bool armed = false;

    for (int fd = 0; fd < n; fd++) {

        short events = 0;

        if (POLL_FDSET_ISSET(fd, in))
            events |= POLL_SET_IN;

        if (POLL_FDSET_ISSET(fd, out))
            events |= POLL_SET_OUT;

        if (POLL_FDSET_ISSET(fd, ex))
            events |= POLL_SET_EX;

        if (!events)
            continue;


        bool one;

        if ((err = poll_arm(fd, events, to, &one)) < 0)
            return err;

        armed |= one;
    }


    if ((ready = __poll_scan_fdset(n, in, out, ex, rin, rout, rex)) < 0)
        return ready;

    if (ready > 0)
        goto answer;


    return poll_suspend(armed, to);
}
