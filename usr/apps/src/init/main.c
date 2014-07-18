//
//  main.c
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/times.h>
#include <time.h>

#include "../../../../src/include/aplus.h"

#include <pthread.h>


clock_t ec;
clock_t sc;


void load_modules() {
	chdir("/ramdisk");
	DIR* rd = (DIR*) opendir("/ramdisk");
	if(!rd) {
		printf("init: could not open ramdisk path\n");
		_exit(-1);
	}
		
	struct dirent* ent;
	while(ent = (struct dirent*) readdir(rd)) {
		char* p = (char*) ((uint32_t)ent->d_name + strlen(ent->d_name) - 3);
		
		if(strcmp(p, ".km") == 0) {
			printf("init: loading %-60s", ent->d_name);

			if(execl(ent->d_name, ent->d_name, 0) == 0)
				printf("[OK]\n");
			else
				printf("[%d]\n", ret);
		}
	}
	
	closedir(rd);
	chdir("/");
}


void load_system() {
	mount("", "/proc", "procfs", NULL, NULL);
	mount("/dev/cd2", "/dev/cdrom", "iso9660", NULL, NULL);

	symlink("/dev/cdrom", "/usr");
}


int main(int argc, char** argv) {
	sc = clock();

	load_modules();
	load_system();

	ec = clock();

	printf("System loaded in %gs\n\n", (double)(ec - sc) / (double) CLOCKS_PER_SEC);

	execlp(getenv("SHELL"), getenv("SHELL"), 0);

	return 0;
}