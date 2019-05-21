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


#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

static int __h_errno = 0;

int* __h_errno_location(void) {
    return &__h_errno;
}


const char* hstrerror(int herrno) {
    static char* errors[] = {
        "Resolver internal error, see errno",
        "Success",
        "Authoritative answer: host not found",
        "Non-authoritative \"host not found\", or SERVERFAIL",
        "Non recoverable errors, FORMERR, REFUSED, NOTIMP",
        "Valid name, no data record of requested type",
        NULL
    };
    
    if(herrno > 4)
        return "Unknown resolver error";
    
    return errors[herrno + 1];
}


void herror(const char* s) {
    if(s)
        fprintf(stderr, "%s: %s\n", s, hstrerror(__h_errno));
    else
        fprintf(stderr, "%s\n", hstrerror(__h_errno));
}
