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

#include <aplus.h>
#include <aplus/debug.h>
#include <stdint.h>

#include "__ubsan.h"



__noreturn
__nosanitize("undefined")
void __ubsan_handle_nonnull_arg(struct nonnull_arg_data* data) {

#if DEBUG_LEVEL_TRACE
    kprintf("ubsan: caught " __FILE__ " exception!\n");
#endif

    DEBUG_ASSERT(data);
    DEBUG_ASSERT(data->location.file);
    DEBUG_ASSERT(data->attribute.file);

    kpanicf("PANIC! UBSAN: null pointer passed as argument %d specified in %s:%d:%d as __attribute__((nonnull)) on %s:%d:%d\n",
        data->arg_index,
        data->attribute.file,
        data->attribute.line,
        data->attribute.column,
        data->location.file,
        data->location.line,
        data->location.column);
        
}
