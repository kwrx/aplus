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


#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>

int utime(const char* filename, const struct utimbuf* times) {
	if(!times)
		return errno = EINVAL, -1;
	

	struct timeval tv[2];
	tv[0].tv_sec = times->actime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = times->modtime;
	tv[1].tv_usec = 0;

	return utimes(filename, tv);
}
