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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/syscall.h>
#include <stdint.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/types.h>

static int run(void) {

    // Create files
    TESTCASE_FAIL(sys_open("testF1.txt", O_CREAT, 0644) < 0);
    TESTCASE_FAIL(sys_open("testF2.txt", O_CREAT, 0644) < 0);
    TESTCASE_FAIL(sys_open("testF3.txt", O_CREAT, 0644) < 0);


    // Open directory
    int fd;
    TESTCASE_FAIL((fd = sys_open("/", O_RDONLY, 0)) < 0);

    struct dirent e;

    // Traverse path
    do {
        int c;
        TESTCASE_FAIL((c = sys_getdents(fd, &e, sizeof(e))) < 0);
    
        if(c == 0)
            break;

        kprintf(" -> %s\n", e.d_name);
    } while(1);

    
    // Close directory
    TESTCASE_FAIL(sys_close(fd) < 0);

    return E_OK;
}


TESTCASE ( sys_getdents, {
    .name = "syscalls/sys_getdents",
    .desc = "Testing syscall sys_getdents(fd, dirent, size)",
    .file = __FILE__,
    .run = &run
});

