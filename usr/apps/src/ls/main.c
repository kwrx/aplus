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


#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>


int main(int argc, char** argv) {

	char* dir = ".";

	if(argc > 1)
		 dir = argv[1];
	
	DIR* d = opendir(dir);
	if(!d)
		exit(-1);
		
	struct dirent* ent;
	while(ent = readdir(d))
		printf("%s\n", ent->d_name);

	return 0;
}