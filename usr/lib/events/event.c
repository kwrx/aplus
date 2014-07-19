//
//  event.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <aplus/ioctl.h>
#include <aplus/events.h>

int event_add(int type) {
	int fd = open("/dev/events", O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return EV_ERROR;
	}

	ioctl(fd, IOCTL_EV_SET, &type);
	close(fd);

	return 0;
}

int event_rem(int type) {
	int fd = open("/dev/events", O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return EV_ERROR;
	}


	ioctl(fd, IOCTL_EV_UNSET, &type);
	close(fd);

	return 0;
}

int event_gettype() {
	int fd = open("/dev/events", O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return EV_ERROR;
	}

	int type;

	ioctl(fd, IOCTL_EV_TYPE, &type);
	close(fd);

	return type;
}

int event_raise(int type) {
	int fd = open("/dev/events", O_RDONLY, 0644);
	if(fd < 0) {
		errno = ENOENT;
		return EV_ERROR;
	}

	ioctl(fd, IOCTL_EV_RAISE, &type);
	close(fd);

	return 0;
}