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

#define __WITH_KINDS
#include "__ubsan.h"



__noreturn
__nosanitize("undefined")
void __ubsan_handle_invalid_builtin(struct invalid_builtin_data* data) {

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("ubsan: caught " __FILE__ " exception!\n");
#endif

    DEBUG_ASSERT(data);
    DEBUG_ASSERT(data->location.file);
    DEBUG_ASSERT(data->kind < sizeof(__kinds) / sizeof(__kinds[0]));

    kpanicf("PANIC! UBSAN: invalid builtin of kind %s on %s:%d:%d\n", __kinds[data->kind],
        data->location.file,
        data->location.line,
        data->location.column);

}