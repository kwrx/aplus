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

SYSCALL(187, sendfile,
ssize_t sys_sendfile(int out, int in, off_t* off, size_t count) {
    if(off && (sys_lseek(in, *off, SEEK_SET) < 0))
        return -1;


    ssize_t w = 0;
    ssize_t r = 0;
    ssize_t i = 0;

    char buf[BUFSIZ];
    for(i = 0; (i + BUFSIZ) < count; i += BUFSIZ) {
        if((r = sys_read(in, buf, BUFSIZ)) < 0)
            return -1;

        w += sys_write(out, buf, r);
        
        if(r != BUFSIZ)
            goto done;
    }


    r = sys_read(in, buf, (count - i));
    w += sys_write(out, buf, r);

done:
    if(off)
        *off = sys_lseek(in, 0, SEEK_CUR);
        
    return w;
});
