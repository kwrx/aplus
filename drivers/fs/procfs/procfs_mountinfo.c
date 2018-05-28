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


#define _WITH_MNTFLAGS 1
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "procfs.h"


static char* __mkpath(volatile task_t* tk, inode_t* d, char* buf, size_t len) {
    if(!d) {
        strcpy(buf, "none");
        return buf;
    }

    memset(buf, 0, len);


    int i = 0;    
    while(d && d != tk->root) {
        int j;
        for(j = strlen(d->name) - 1; j >= 0; j--)
            buf[i++] = d->name[j];

        buf[i++] = '/';
        d = d->parent;
    }

    

    if(strlen(buf) > 0) {
        int j, k;
        for(j = strlen(buf) - 1, i = 0, k = 0; k < strlen(buf) / 2; k++, j--, i++) {
            buf[i] ^= buf[j];
            buf[j] ^= buf[i];
            buf[i] ^= buf[j];
        }
    } else 
        buf[0] = '/';

    return buf;
}

static char* __mkopt(int flags, char* buf, size_t len) {
    memset(buf, 0, len);
    
    int i;
    for(i = 0; mnt_flags[i].option; i++) {
        if(flags & mnt_flags[i].value) {
            strcat(buf, mnt_flags[i].option);
            strcat(buf, ",");
        }
    }

    if(strlen(buf) > 0)
        buf[strlen(buf) - 1] = '\0';
    else
        strcpy(buf, "defaults");

    return buf;
}


int procfs_mountinfo_init(procfs_entry_t* e) {
    return 0;
}


int procfs_mountinfo_update(procfs_entry_t* e) {
    char d[BUFSIZ];
    char r[BUFSIZ];
    char o[256];

    volatile task_t* tk = e->task;
    if(unlikely(!tk))
        tk = current_task;

    char* p = e->data;
    list_each(mnt_queue, mt) {
        __mkpath(tk, mt->root, d, sizeof(d));
        __mkpath(tk, mt->info->dev, r, sizeof(r));
        __mkopt(mt->info->flags, o, sizeof(o));

        sprintf (p, 
            "%d %d %d:%d %s %s %s - %s %s\n",
            (int) (mt->id),
            (int) (mt->parent ? mt->parent->id : 0),
            mt->info->dev ? mt->info->dev->dev : 0,
            mt->info->dev ? mt->info->dev->dev : 0,
            d, d, o,
            mt->info->dev ? r : "none",
            o
        );

        p += strlen(p);
    }

    e->size = strlen(e->data);
    return 0;
}