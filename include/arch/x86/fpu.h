/*                                                                      
 * Author(s):                                                           
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
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
                                                                        


#ifndef _APLUS_X86_FPU_H
#define _APLUS_X86_FPU_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>


__BEGIN_DECLS

void fpu_init(uint64_t);
void fpu_switch(void*, void*);
void fpu_save(void* fpu_area);
void fpu_restore(void* fpu_area);
void* fpu_new_state(void);
void fpu_free_state(void*);
size_t fpu_size(void);

__END_DECLS

#endif
#endif
