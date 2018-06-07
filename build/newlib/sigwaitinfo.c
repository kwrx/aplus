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


#include <signal.h>

/* CRT0 */
extern int __last_signo;

int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
    sigset_t old;
    sigprocmask(SIG_SETMASK, set, &old);

    if(timeout)
        nanosleep(timeout, NULL);
    else
        pause();

    sigprocmask(SIG_SETMASK, &old, NULL);
    return __last_signo;
}

int sigwaitinfo(const sigset_t* set, siginfo_t* info) {
    return sigtimedwait(set, info, NULL);
}