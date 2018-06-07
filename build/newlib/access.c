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


#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


static int __check(struct stat* st, int flags, int rdflags, int wrflags, int exflags) {
	if(flags & R_OK)
		if(!(st->st_mode & rdflags))
			return -1;

	if(flags & W_OK)
		if(!(st->st_mode & wrflags))
			return -1;

	if(flags & X_OK)
		if(!(st->st_mode & exflags))
			return -1;

	return 0;
}


int access(const char* pathname, int flags) {
	struct stat st;
	if(stat(pathname, &st) != 0)
		return -1;

	if(geteuid() == st.st_uid)
		return __check(&st, flags, S_IRUSR, S_IWUSR, S_IXUSR);
	if(getegid() == st.st_gid)
		return __check(&st, flags, S_IRGRP, S_IWGRP, S_IXGRP);

	return __check(&st, flags, S_IROTH, S_IWOTH, S_IXOTH);
}
