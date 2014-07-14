//
//  nanosleep.c
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

#include <sys/times.h>
#include <sys/time.h>
#include <errno.h>


int nanosleep(const struct timespec* req, struct timespec* rem) {
	if(req->tv_sec > 999999999) {
		errno = EINVAL;
		return -1;
	}

	struct timeval tm;
	struct timezone* tz;

	if(gettimeofday(&tm, &tz) != 0)
			return -1;

	int t0 = req->tv_sec + tm.tv_sec;
	int t1 = req->tv_nsec + tm.tv_usec;

#ifdef APLUS
	aplus_thread_idle();
#endif

	while(1) {
		if(gettimeofday(&tm, &tz) != 0)
			return -1;

		if(tm.tv_sec > t0 && tm.tv_usec > t1)
			break;
	}

#ifdef APLUS
	aplus_thread_wakeup();
#endif

	return 0;
}