#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "ext2.h"


int ext2_open(struct inode* inode) {
    ext2_t* priv = (ext2_t*) inode->userdata;
    if(!priv) {
        errno = EINVAL;
        return E_ERR;
    }


    ext2_inode_t e;
    ext2_read_inode(priv, &e, (uint32_t) inode->ino);

    for(int i = 0; i < 12; i++) {
        if(!e.dbp[i])
            break;



        ext2_dir_t* d = (ext2_dir_t*) kmalloc(priv->blocksize, GFP_KERNEL);
        ext2_read_block(priv, d, e.dbp[i]);


        for(; d->inode; ) {
            if(d->name[0] == '.' && d->namelength == 1)
                goto done;

            if(d->name[0] == '.' && d->name[1] == '.' && d->namelength == 2)
                goto done;
                

            char name[BUFSIZ];
            memset(name, 0, sizeof(name));
            memcpy(name, &d->name, d->namelength);


            struct inode_childs* cx;
            for(cx = inode->childs; cx; cx = cx->next)
                if(strcmp(cx->inode->name, name) == 0)
                    goto done;


            inode_t* child;
            ext2_mkchild(priv, &child, inode, d->inode, name);


            cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
            cx->inode = child;
            cx->next = inode->childs;
            inode->childs = cx;

done:
            d = (ext2_dir_t*) ((uintptr_t) d + d->size);
        }

        kfree(d);
    }
    
    return E_OK;
}