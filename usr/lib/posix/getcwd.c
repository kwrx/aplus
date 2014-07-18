//
//  getcwd.c
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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static char* strcat_inv(char* dest, char* str) {
	char* res = malloc(strlen(dest) + strlen(str));
	strcpy(res, str);	
	strcat(res, dest);
	strcpy(dest, res);

	free(res);
	return dest;
}

char* getcwd(char* buf, size_t size) {
	struct stat st;
	char tmpbuf[1024];

	int root_ino = 0;
	int curr_ino = 0;

	if(stat("/", &st) != 0)
		return NULL;
	root_ino = st.st_ino;

	if(stat(".", &st) != 0)
		return NULL;
	curr_ino = st.st_ino;


	memset(buf, 0, size);
	memset(tmpbuf, 0, 1024);
	strcat(tmpbuf, ".");

	if(curr_ino == root_ino) {
		strcat_inv(buf, "/");
		return buf;
	}


	while(curr_ino != root_ino) {
		if(stat(tmpbuf, &st) != 0)
			return NULL;

		curr_ino = st.st_ino;

		if(curr_ino == root_ino)
			break;

		strcat(tmpbuf, "/..");
		DIR* dir = opendir(tmpbuf);
		if(!dir)
			return NULL;

		struct dirent* ent;
		while(ent = readdir(dir)) {
			if(ent->d_ino == curr_ino) {
				strcat_inv(buf, ent->d_name);
				strcat_inv(buf, "/");			
			}

			free(ent);
		}

		closedir(dir);
	}

	return buf;
}