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

    // Create file
    int fd;
    TESTCASE_FAIL((fd = sys_open("/testIO.txt", O_CREAT | O_EXCL | O_WRONLY, S_IFREG | 0644)) < 0);

    // Write file
    TESTCASE_FAIL(sys_write(fd, "Hello World", 12) <= 0);

    // Close file
    TESTCASE_FAIL(sys_close(fd) < 0);

    
    // Open file
    TESTCASE_FAIL((fd = sys_open("/testIO.txt", O_RDONLY, 0)) < 0);


    char buf[32] = { 0 };

    // Write file
    int e;
    TESTCASE_FAIL((e = sys_read(fd, buf, sizeof(buf))) <= 0);

    // Output content
    kprintf(" -> %d: '%s'\n", e, buf);

    // Close file
    TESTCASE_FAIL(sys_close(fd) < 0);


    return E_OK;
}


TESTCASE ( sys_read_write, {
    .name = "syscalls/sys_read_write",
    .desc = "Testing syscall sys_read/write(fd, buf, size)",
    .file = __FILE__,
    .run = &run
});
