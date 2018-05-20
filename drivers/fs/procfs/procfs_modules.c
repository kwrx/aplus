#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/module.h>
#include <libc.h>

#include "procfs.h"


int procfs_modules_init(procfs_entry_t* e) {
    return 0;
}


int procfs_modules_update(procfs_entry_t* e) {
    memset(e->data, 0, strlen(e->data));

    list_each(m_queue, tmp) {
        char buf[128];
        memset(buf, 0, sizeof(buf));

        sprintf(buf, 
            "%s %d %d ",
            tmp->name,
            tmp->size,
            tmp->loaded
        );
        strcat(e->data, (const char*) buf);

        if(list_length(tmp->deps) == 0)
            strcat(e->data, "-");
        else {
            list_each(tmp->deps, d) {
                strcat(e->data, d);
                strcat(e->data, ",");
            }
        }

        memset(buf, 0, sizeof(buf));
        sprintf(buf, " Live %p\n", tmp->loaded_address);
        strcat(e->data, (const char*) buf);
    }
    

    e->size = strlen(e->data);
    return 0;
}