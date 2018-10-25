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


#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>


static int __check(struct stat* st, int flags, int rdflags, int wrflags, int exflags) {
	if(flags & R_OK)
		if(!(st->st_mode & rdflags))
			goto noperm;

	if(flags & W_OK)
		if(!(st->st_mode & wrflags))
			goto noperm;

	if(flags & X_OK)
		if(!(st->st_mode & exflags))
			goto noperm;

	return 0;

noperm:
	errno = EACCES;
	return -1;
}

SYSCALL(33, access,
int sys_access(const char* pathname, int flags) {
    struct stat st;
	if(sys_stat(pathname, &st) != 0)
		return -1;

    if(current_task->uid == st.st_uid)
		return __check(&st, flags, S_IRUSR, S_IWUSR, S_IXUSR);
	if(current_task->gid == st.st_gid)
		return __check(&st, flags, S_IRGRP, S_IWGRP, S_IXGRP);

	return __check(&st, flags, S_IROTH, S_IWOTH, S_IXOTH);
});