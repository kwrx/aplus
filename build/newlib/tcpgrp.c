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


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <errno.h>

pid_t tcgetpgrp(int fd) {
	pid_t pid;
	if(ioctl(fd, TIOCGPGRP, &pid) != 0)
		return -1;

	return pid;
}

int tcsetpgrp(int fd, pid_t pid) {
	return ioctl(fd, TIOCSPGRP, &pid);
}

int tcsetattr(int fd, int flags, const struct termios* p) {
    switch(flags) {
        case TCSANOW:
            return ioctl(fd, TIOCSETA, p);
        case TCSADRAIN:
            return ioctl(fd, TIOCSETAW, p);
        case TCSAFLUSH:
            return ioctl(fd, TIOCSETAF, p);
        default:
            break;
    }

    errno = EINVAL;
    return -1;
}

int tcgetattr(int fd, struct termios* p) {
    return ioctl(fd, TIOCGETA, p);
}

int tcdrain(int fd) {
	return ioctl(fd, TIOCDRAIN, NULL);
}

int tcflush(int fd, int mode) {
	return ioctl(fd, TIOCFLUSH, &mode);
}

int tcflow(int fd, int action) {
	switch(action) {
		case TCOOFF:
		case TCIOFF:
			return ioctl(fd, TIOCSTOP, NULL);
		case TCOON:
		case TCION:
			return ioctl(fd, TIOCSTART, NULL);
		default:
			break;
	}

	errno = EINVAL;
	return -1;
}

int tcsendbreak(int fd, int duration) {
	return;
}
