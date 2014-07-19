//
//  alarm.c
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

#include <unistd.h>
#include <signal.h>
#include <errno.h>

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

int alarm_thread(int seconds) {
	sleep(seconds);
	raise(SIGALRM);

	return 0;
}

unsigned alarm(unsigned seconds) {
#ifdef USE_PTHREAD
	static pthread_t thr = -1;

	if(thr != -1)
		if(pthread_detach(thr) != 0)
			return -1;

	if(seconds > 0)
		if(pthread_create(&thr, NULL, alarm_thread, seconds) != 0)
			return -1;
	return 0;

#else
	errno = ENOSYS;
	return -1;
#endif
}