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


#include "appx.h"


void appx_lck_acquire() {
    int fd = open(APPX_LOCK, O_CREAT | O_EXCL, 0644 | S_IFREG);
    if(fd < 0) {
        fprintf(stderr, "appx: another instance of APPX is running, wait or remove lock manually:\n\trm -f %s\n", APPX_LOCK);
        abort();
    }

    close(fd);
}


void appx_lck_release() {
    if(unlink(APPX_LOCK) == 0)
        return;

    perror("appx: error on releasing lock file");
    fprintf(stderr, "Please try to remove manually:\n\trm -f %s\n", APPX_LOCK);
}