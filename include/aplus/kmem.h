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


#ifndef _APLUS_KMEM_H
#define _APLUS_KMEM_H

#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#include <aplus/base.h>



#ifdef __cplusplus
extern "C" {
    int ioctl(int, int, void*);
}
#endif


#define KMEMIOCTL_ALLOC          1
#define KMEMIOCTL_FREE           2




static inline void* kmem_alloc(size_t count) {
    int fd = open(PATH_KMEM, O_RDONLY);
    if(fd < 0)
        return NULL;
        
    void* p = (void*) count;
    if(ioctl(fd, KMEMIOCTL_ALLOC, &p) != 0)
        p = NULL;
        
    close(fd);
    return p;
}

static inline void kmem_free(void* ptr) {
    int fd = open(PATH_KMEM, O_RDONLY);
    if(fd < 0)
        return;
        
    
    ioctl(fd, KMEMIOCTL_FREE, ptr);   
    close(fd);
}




#ifdef __cplusplus


#ifdef KMEM_INCLUDE_OPERATOR
void* operator new(size_t count, int n0, int n1) {
    (void) n0;
    (void) n1;
    return kmem_alloc(count);
}

void* operator new[](size_t count, int n0, int n1) {
    (void) n0;
    (void) n1;
    return kmem_alloc(count);
}

void operator delete(void* ptr, int n0, int n1) {
    (void) n0;
    (void) n1;
    return kmem_free(ptr);
}

void operator delete[](void* ptr, int n0, int n1) {
    (void) n0;
    (void) n1;
    return kmem_free(ptr);
}
#else
void* operator new(size_t count, int n0, int n1);
void* operator new[](size_t count, int n0, int n1);
void operator delete(void* ptr, int n0, int n1);
void operator delete[](void* ptr, int n0, int n1);
#endif


#define knew new(0, 0)
#define kdelete delete(0, 0)


#endif
#endif