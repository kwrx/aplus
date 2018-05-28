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


#include "pthread_internal.h"


static int __pthread_routine(void* arg) {
    struct p_context* cc = (struct p_context*) arg;
    if(!cc)
        pthread_exit(NULL);

    pthread_exit(cc->start_routine(cc->arg));
}

int pthread_create(pthread_t* th, const pthread_attr_t* attr, void* (*start_routine) (void*), void* arg) {
    __pthread_init();
    
    struct p_context* cc = (struct p_context*) calloc(1, sizeof(struct p_context));
    if(!cc) {
        errno = ENOMEM;
        return -1;
    }

    if(attr)
        memcpy(&cc->attr, &attr, sizeof(pthread_attr_t));

    cc->start_routine = start_routine;
    cc->arg = arg;
    cc->pid = clone(__pthread_routine, cc->attr.stackaddr, CLONE_FS | CLONE_FILES | CLONE_VM | CLONE_SIGHAND, cc);

    if(cc->pid < 0) {
        free(cc);
        return -1;
    }


    __pthread_add_queue(cc);

    (*th) = (pthread_t) cc;
    return 0;
}

