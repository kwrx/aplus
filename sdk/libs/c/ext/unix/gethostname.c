/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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


#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

int
gethostname (char *name, size_t len)
{
	struct utsname nodebuf;
	size_t nodelen;

	if (uname (&nodebuf))
		return -1;

	nodelen = strlen (nodebuf.nodename) + 1;
	if (len < nodelen)
		memcpy (name, nodebuf.nodename, len);
	else
		memcpy (name, nodebuf.nodename, nodelen);

	if (nodelen > len)
	{
		_REENT->_errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

