/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aplus.                                          
 *                                                                      
 * aplus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aplus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>



static inode_t* path_find(inode_t* inode, const char* path, size_t size) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(path);
    DEBUG_ASSERT(size);


    char s[size + 1]; 
    memset(s, 0, size + 1);

    strncpy(s, path, size);


    if((inode = vfs_finddir(inode, s)) == NULL)
        return NULL;


    struct stat st = { 0 };

    if(vfs_getattr(inode, &st) < 0) {
        return NULL;
    }

    if(S_ISLNK(st.st_mode)) {
        inode = path_follows(inode);
    }

    return inode;
    
}



inode_t* path_follows(inode_t* inode) {

    DEBUG_ASSERT(inode);

    char s[CONFIG_PATH_MAX + 1] = { 0 };

    if(vfs_readlink(inode, s, CONFIG_PATH_MAX) <= 0)
        return NULL;

    return path_lookup(inode->parent, s, O_RDONLY, 0);

}



inode_t* path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode) {

    DEBUG_ASSERT(cwd);
    DEBUG_ASSERT(path);


    inode_t* c = NULL;

    if(path[0] == '/') {

        shared_ptr_access(current_task->fs, fs, {
            c = fs->root;
        });

    } else {

        c = cwd;

    }

    DEBUG_ASSERT(c);


    while(path[0] == '/') {
        path++;
    }

    while(strchr(path, '/') && c) {
         
        c = path_find(c, path, strcspn(path, "/")); 
        path = strchr(path, '/') + 1;

        while(path[0] == '/')
            path++;
    
    }


    if(unlikely(!c)) {
        return errno = ENOENT, NULL;
    }
    
    inode_t* r;

    if(path[0] != '\0')
        r = path_find(c, path, strlen(path));
    else
        r = c;


    if(unlikely(!r)) {
        
        if(flags & O_CREAT) {
        
            if((mode & S_IFMT) == 0) {
                mode |= S_IFREG;
            }

            r = vfs_creat(c, path, mode);

            if(unlikely(!r))
                return NULL;
        
        } else {
            return errno = ENOENT, NULL;
        }

    } else {
     
        if((flags & O_EXCL) && (flags & O_CREAT))
            return errno = EEXIST, NULL;
    
    }

    return r;

}