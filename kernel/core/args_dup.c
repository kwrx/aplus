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


#include <aplus.h>
#include <aplus/mm.h>
#include <libc.h>

char** args_dup(char** args) {
    int len = 0;
    char** ret = NULL;
    
    if(unlikely(!args)) {
        ret = (char**) kmalloc(sizeof(char**), GFP_USER);
        ret[0] = NULL;
        
        return ret;    
    }
    
    while(args[len])
        len++;

    ret = (char**) kmalloc(sizeof(char**) * (len + 1), GFP_USER);
    
    int i;
    for(i = 0; i < len; i++)
        ret[i] = strdup(args[i]);
        
    ret[len] = NULL;
    return ret;
}