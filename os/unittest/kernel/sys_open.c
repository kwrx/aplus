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
    TESTCASE_FAIL(sys_open("/testA.txt", O_CREAT | O_EXCL, 0644) < 0);
    TESTCASE_FAIL(sys_open("./testB.txt", O_CREAT | O_EXCL, 0644) < 0);
    TESTCASE_FAIL(sys_open("testC.txt", O_CREAT | O_EXCL, 0644) < 0);

    // Already exists
    TESTCASE_FAIL(sys_open("testC.txt", O_CREAT | O_EXCL, 0644) != -EEXIST);
    
    // Wrong path
    TESTCASE_FAIL(sys_open("/../testD.txt", O_CREAT | O_EXCL, 0644) != -ENOENT);
    
    // Wrong path
    TESTCASE_FAIL(sys_open("directory/testC.txt", O_CREAT | O_EXCL, 0644) != -ENOENT);

    
    return E_OK;
}


TESTCASE ( sys_open, {
    .name = "syscalls/sys_open",
    .desc = "Testing syscall sys_open(name, flags, mode)",
    .file = __FILE__,
    .run = &run
});

