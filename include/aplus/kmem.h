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