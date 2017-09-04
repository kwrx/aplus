#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

#include "iso9660.h"



void iso9660_checkname(char* name) {
    char* p = strchr(name, ';');
    if(likely(p)) {
        *p-- = 0;
    
        if(*p == '.')
            *p = 0;
    }

    for(int i = 0; i < strlen(name); i++)
        if(name[i] >= 'A' && name[i] <= 'Z')
            name[i] += 32;
}


uint16_t iso9660_getmsb16(uint32_t val) {
    return (val >> 16) & 0xFFFF;
}

uint32_t iso9660_getmsb32(uint64_t val) {
    return (val >> 32) & 0xFFFFFFFF;
}

uint16_t iso9660_getlsb16(uint32_t val) {
    return (val >> 0) & 0xFFFF;
}

uint32_t iso9660_getlsb32(uint64_t val) {
    return (val >> 0) & 0xFFFFFFFF;
}

#if HAVE_ROCKRIDGE
char* rockridge_getname(void* offset) {        
    struct {
        char type[2];
        uint8_t length;
        uint8_t sysver;
        uint8_t flags;
        uint8_t name[0];
    } __packed *h = offset;
    
                            
    while(strncmp(h->type, "NM", 2) != 0)
        h = (void*) ((uintptr_t) h + h->length);
        
    char* name = (char*) kmalloc(h->length - sizeof(*h) + 1, GFP_USER);
    KASSERT(name);
    
    memset(name, 0, h->length - sizeof(*h) + 1);
    strncpy((char*) name, h->name, h->length - sizeof(*h));
    return name;
}

void rockridge_getmode(void* offset, mode_t* mode, uid_t* uid, gid_t* gid, nlink_t* nlink) {
    struct {
        char type[2];
        uint8_t length;
        uint8_t sysver;
        uint64_t mode;
        uint64_t nlink;
        uint64_t uid;
        uint64_t gid;
        uint64_t ino;
    } __packed *h = offset;
    
                            
    while(strncmp(h->type, "PX", 2) != 0)
        h = (void*) ((uintptr_t) h + h->length);
        
    *mode = (mode_t) h->mode;
    *uid = (uid_t) h->uid;
    *gid = (gid_t) h->gid;
    *nlink = (nlink_t) h->nlink;
}
#endif
