//
//  getgid.c
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
#include <sys/stat.h>


gid_t getgid(void) {
	struct stat st;
	if(fstat(STDIN_FILENO, &st) != 0)
		return -1;

	return st.st_gid;
}

gid_t getegid(void) {
	return getgid();
}

int getgroups(int len, gid_t* list) {
	return -1;
}