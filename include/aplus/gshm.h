#ifndef _APLUS_GSHM_H
#define _APLUS_GSHM_H

#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#include <aplus/base.h>



#ifdef __cplusplus
extern "C" {
    int ioctl(int, int, void*);
}
#endif


#define GSHMIOCTL_ALLOC          1
#define GSHMIOCTL_FREE           2




static inline void* galloc(size_t count) {
    int fd = open(PATH_GHSM, O_RDONLY);
    if(fd < 0)
        return NULL;
        
    void* p = (void*) count;
    if(ioctl(fd, GSHMIOCTL_ALLOC, &p) != 0)
        p = NULL;
        
    close(fd);
    return p;
}

static inline void gfree(void* ptr) {
    int fd = open(PATH_GHSM, O_RDONLY);
    if(fd < 0)
        return;
        
    
    ioctl(fd, GSHMIOCTL_FREE, ptr);   
    close(fd);
}




#ifdef __cplusplus


#ifdef GSHM_INCLUDE_OPERATOR
void* operator new(size_t count, int n0, int n1) {
    return galloc(count);
}

void* operator new[](size_t count, int n0, int n1) {
    return galloc(count);
}

void operator delete(void* ptr, int n0, int n1) {
    return gfree(ptr);
}

void operator delete[](void* ptr, int n0, int n1) {
    return gfree(ptr);
}
#else
void* operator new(size_t count, int n0, int n1);
void* operator new[](size_t count, int n0, int n1);
void operator delete(void* ptr, int n0, int n1);
void operator delete[](void* ptr, int n0, int n1);
#endif


#define gnew new(0, 0)
#define gdelete delete(0, 0)


#endif
#endif