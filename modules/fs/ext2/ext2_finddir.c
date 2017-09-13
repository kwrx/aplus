#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "ext2.h"


inode_t* ext2_finddir(struct inode* inode, char* filename) {
    ext2_priv_t* priv = (ext2_priv_t*) inode->userdata;
    if(!priv) {
        errno = EINVAL;
        return NULL;
    }

    for(int i = 0; i < 12; i++) {
        if(!priv->inode.dbp[i])
            break;



        ext2_dir_t* d = (ext2_dir_t*) kmalloc(priv->ext2->blocksize, GFP_KERNEL);
        ext2_read_block(priv->ext2, d, priv->inode.dbp[i]);


        for(; d->inode; ) {
            if(d->name[0] == '.' && d->namelength == 1)
                goto done;

            if(d->name[0] == '.' && d->name[1] == '.' && d->namelength == 2)
                goto done;
                

            char name[BUFSIZ];
            memset(name, 0, sizeof(name));
            memcpy(name, &d->name, d->namelength);

            if(strcmp(name, filename) != 0)
                goto done;

            
            inode_t* child;
            ext2_mkchild(priv->ext2, &child, inode, d->inode, name);


            struct inode_childs* cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
            cx->inode = child;
            cx->next = inode->childs;
            inode->childs = cx;

            return child;
done:
            d = (ext2_dir_t*) ((uintptr_t) d + d->size);
        }

        kfree(d);
    }
    
    errno = ENOENT;
    return NULL;
}